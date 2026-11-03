#!/usr/bin/env python3
"""Create an offline outer-boundary SIGHT.MAD candidate from a user-supplied file.

This script contains no game assets and never edits its input. It updates only
the four vertices on the largest boundary loop, expanding them about that loop's
center. The original circular aperture and all archive metadata stay byte exact.
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import json
import math
import struct
from pathlib import Path


def parse_archive(blob: bytes):
    if len(blob) < 24:
        raise ValueError("archive too short")
    table_end = struct.unpack_from("<I", blob, 16)[0]
    if table_end < 24 or table_end > len(blob) or table_end % 24:
        raise ValueError("invalid archive directory end")
    entries = []
    for at in range(0, table_end, 24):
        name = blob[at:at + 16].split(b"\0", 1)[0]
        offset, size = struct.unpack_from("<II", blob, at + 16)
        if offset > len(blob) or size > len(blob) - offset:
            raise ValueError("archive entry outside file")
        entries.append({"name": name, "offset": offset, "size": size})
    return entries


def find_entry(entries, suffix: bytes):
    matches = [e for e in entries if e["name"].lower().endswith(suffix)]
    if len(matches) != 1:
        raise ValueError(f"expected one {suffix.decode()} entry, got {len(matches)}")
    return matches[0]


def parse_fac(fac: bytes):
    if len(fac) < 24:
        raise ValueError("short FAC")
    triangle_count = struct.unpack_from("<I", fac, 16)[0]
    at = 20
    triangles = []
    for _ in range(triangle_count):
        if at + 32 > len(fac):
            raise ValueError("truncated FAC triangle")
        # UV bytes are the first six bytes; three signed-short vertex indices
        # begin at +6 in the known PC FAC layout.
        ids = struct.unpack_from("<3h", fac, at + 6)
        triangles.append(ids)
        at += 32
    if at + 4 > len(fac):
        raise ValueError("missing FAC quad count")
    quad_count = struct.unpack_from("<I", fac, at)[0]
    at += 4
    if quad_count:
        raise ValueError("unexpected quads; refusing an unverified geometry layout")
    if at != len(fac):
        raise ValueError("unexpected trailing FAC data")
    return triangles


def boundary_loops(vtx: bytes, triangles):
    if len(vtx) % 8:
        raise ValueError("VTX length is not a multiple of 8")
    verts = list(struct.iter_unpack("<4h", vtx))
    edge_counts = collections.Counter()
    for tri in triangles:
        if len(set(tri)) != 3 or min(tri) < 0 or max(tri) >= len(verts):
            raise ValueError("invalid triangle vertex index")
        for a, b in zip(tri, (tri[1], tri[2], tri[0])):
            edge_counts[tuple(sorted((a, b)))] += 1
    if any(n > 2 for n in edge_counts.values()):
        raise ValueError("non-manifold edge; refusing to patch")
    adj = collections.defaultdict(set)
    for (a, b), count in edge_counts.items():
        if count == 1:
            adj[a].add(b)
            adj[b].add(a)
    if not adj or any(len(ns) != 2 for ns in adj.values()):
        raise ValueError("boundary is not a collection of closed simple loops")
    loops = []
    remaining = set(adj)
    while remaining:
        start = min(remaining)
        loop = [start]
        prev = None
        cur = start
        while True:
            choices = sorted(adj[cur] - ({prev} if prev is not None else set()))
            nxt = choices[0]
            if nxt == start:
                break
            if nxt in loop:
                raise ValueError("boundary self-intersects or branches")
            loop.append(nxt)
            prev, cur = cur, nxt
        if len(loop) < 3:
            raise ValueError("boundary loop too short")
        remaining.difference_update(loop)
        loops.append(loop)
    return verts, loops


def signed_area(loop, verts):
    pts = [(verts[i][1], verts[i][2]) for i in loop]
    return sum(x1 * y2 - x2 * y1 for (x1, y1), (x2, y2) in
               zip(pts, pts[1:] + pts[:1])) / 2.0


def make_candidate(original: bytes, scale: float):
    if not math.isfinite(scale) or scale <= 1.0 or scale > 4.0:
        raise ValueError("scale must be >1 and <=4")
    entries = parse_archive(original)
    vtx_entry = find_entry(entries, b".vtx")
    fac_entry = find_entry(entries, b".fac")
    vtx = original[vtx_entry["offset"]:vtx_entry["offset"] + vtx_entry["size"]]
    fac = original[fac_entry["offset"]:fac_entry["offset"] + fac_entry["size"]]
    triangles = parse_fac(fac)
    verts, loops = boundary_loops(vtx, triangles)
    # The intended sight model has exactly an outer perimeter and an aperture.
    # Requiring two loops and selecting by absolute polygon area avoids guessing
    # from vertex IDs or a hard-coded count.
    if len(loops) != 2:
        raise ValueError(f"expected outer and aperture loops, found {len(loops)}")
    areas = [abs(signed_area(loop, verts)) for loop in loops]
    outer_i = max(range(len(loops)), key=areas.__getitem__)
    outer = loops[outer_i]
    inner = loops[1 - outer_i]
    # Ensure the outer loop is a four-corner rectangle in this source model.
    if len(outer) != 4:
        raise ValueError(f"expected 4 outer boundary vertices, got {len(outer)}")
    outer_pts = [(verts[i][1], verts[i][2]) for i in outer]
    center_y = sum(p[0] for p in outer_pts) / len(outer_pts)
    center_z = sum(p[1] for p in outer_pts) / len(outer_pts)

    changed = bytearray(vtx)
    moved = []
    for i in outer:
        x, y, z, w = verts[i]
        ny = round(center_y + (y - center_y) * scale)
        nz = round(center_z + (z - center_z) * scale)
        if not (-32768 <= ny <= 32767 and -32768 <= nz <= 32767):
            raise ValueError("scaled outer vertex exceeds signed-short bounds")
        struct.pack_into("<4h", changed, i * 8, x, ny, nz, w)
        moved.append({"index": i, "before_yz": [y, z], "after_yz": [ny, nz]})

    candidate = bytearray(original)
    start = vtx_entry["offset"]
    candidate[start:start + len(changed)] = changed
    # Structural invariant: only the four outer-boundary vertex records differ.
    after_entries = parse_archive(candidate)
    if [(e["name"], e["offset"], e["size"]) for e in entries] != [
            (e["name"], e["offset"], e["size"]) for e in after_entries]:
        raise AssertionError("archive directory changed")
    for e in entries:
        if e is vtx_entry:
            continue
        a = original[e["offset"]:e["offset"] + e["size"]]
        b = candidate[e["offset"]:e["offset"] + e["size"]]
        if a != b:
            raise AssertionError(f"non-VTX entry changed: {e['name']!r}")
    # Re-parse and prove the inner/aperture vertex coordinates are unchanged.
    new_verts, new_loops = boundary_loops(bytes(changed), triangles)
    new_inner = min(new_loops, key=lambda loop: abs(signed_area(loop, new_verts)))
    if set(inner) != set(new_inner):
        raise AssertionError("aperture boundary topology changed")
    if any(verts[i] != new_verts[i] for i in inner):
        raise AssertionError("aperture boundary coordinates changed")
    if triangles != parse_fac(fac):
        raise AssertionError("triangle topology changed")
    def orient(ids, points):
        (x1, y1), (x2, y2), (x3, y3) = [(points[i][1], points[i][2]) for i in ids]
        return (x2 - x1) * (y3 - y1) - (y2 - y1) * (x3 - x1)
    for tri in triangles:
        old_sign = orient(tri, verts)
        new_sign = orient(tri, new_verts)
        if old_sign == 0 or new_sign == 0 or (old_sign > 0) != (new_sign > 0):
            raise AssertionError("triangle became degenerate or flipped orientation")
    return bytes(candidate), {
        "scale": scale,
        "outer_loop_indices": sorted(outer),
        "inner_loop_indices": sorted(inner),
        "outer_before_area": areas[outer_i],
        "outer_after_area": abs(signed_area(outer, new_verts)),
        "inner_area_before_after": abs(signed_area(inner, verts)),
        "moved_vertices": moved,
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input_mad", type=Path, help="user-supplied original SIGHT.MAD")
    ap.add_argument("output_mad", type=Path, help="new candidate path; input is never overwritten")
    ap.add_argument("--scale", type=float, default=2.0)
    args = ap.parse_args()
    if args.input_mad.resolve() == args.output_mad.resolve():
        raise SystemExit("refusing to overwrite input")
    src = args.input_mad.read_bytes()
    dst, details = make_candidate(src, args.scale)
    args.output_mad.parent.mkdir(parents=True, exist_ok=True)
    args.output_mad.write_bytes(dst)
    report = {
        "input_sha256": hashlib.sha256(src).hexdigest(),
        "candidate_sha256": hashlib.sha256(dst).hexdigest(),
        "input_bytes": len(src),
        "candidate_bytes": len(dst),
        **details,
    }
    report_path = args.output_mad.with_suffix(args.output_mad.suffix + ".json")
    report_path.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
