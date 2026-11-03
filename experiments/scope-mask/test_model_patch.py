#!/usr/bin/env python3
"""Offline structural tests; pass a user-owned original SIGHT.MAD as argument."""
import hashlib
import sys
import unittest
from pathlib import Path

from build_sight_candidate import make_candidate, parse_archive, find_entry, parse_fac, boundary_loops, signed_area


class SightCandidateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if len(sys.argv) != 2:
            raise RuntimeError("usage: python3 test_model_patch.py /path/to/original/SIGHT.MAD")
        cls.path = Path(sys.argv[1])
        cls.original = cls.path.read_bytes()
        cls.candidate, cls.details = make_candidate(cls.original, 2.0)

    def test_input_digest_and_length_unchanged(self):
        self.assertEqual(len(self.original), len(self.candidate))
        self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(), hashlib.sha256(self.original).digest())

    def test_archive_directory_and_non_vtx_entries_identical(self):
        old = parse_archive(self.original)
        new = parse_archive(self.candidate)
        self.assertEqual([(e["name"], e["offset"], e["size"]) for e in old],
                         [(e["name"], e["offset"], e["size"]) for e in new])
        vtx = find_entry(old, b".vtx")
        for entry in old:
            if entry is vtx:
                continue
            a = self.original[entry["offset"]:entry["offset"] + entry["size"]]
            b = self.candidate[entry["offset"]:entry["offset"] + entry["size"]]
            self.assertEqual(a, b, entry["name"])

    def test_exactly_four_outer_vertices_move_and_inner_stays(self):
        old_entry = find_entry(parse_archive(self.original), b".vtx")
        new_entry = find_entry(parse_archive(self.candidate), b".vtx")
        old_vtx = self.original[old_entry["offset"]:old_entry["offset"] + old_entry["size"]]
        new_vtx = self.candidate[new_entry["offset"]:new_entry["offset"] + new_entry["size"]]
        before = list(__import__("struct").iter_unpack("<4h", old_vtx))
        after = list(__import__("struct").iter_unpack("<4h", new_vtx))
        moved = {i for i, (a, b) in enumerate(zip(before, after)) if a != b}
        self.assertEqual(moved, set(self.details["outer_loop_indices"]))
        self.assertEqual(len(moved), 4)
        self.assertEqual(set(self.details["outer_loop_indices"]) & set(self.details["inner_loop_indices"]), set())
        self.assertEqual(self.details["outer_after_area"], self.details["outer_before_area"] * 4)
        self.assertEqual(self.details["inner_area_before_after"], 178840.5)

    def test_triangle_topology_and_uv_blob_are_identical(self):
        old_entries = parse_archive(self.original)
        old_fac = find_entry(old_entries, b".fac")
        new_fac = find_entry(parse_archive(self.candidate), b".fac")
        a = self.original[old_fac["offset"]:old_fac["offset"] + old_fac["size"]]
        b = self.candidate[new_fac["offset"]:new_fac["offset"] + new_fac["size"]]
        self.assertEqual(a, b)
        self.assertEqual(len(parse_fac(a)), 46)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
