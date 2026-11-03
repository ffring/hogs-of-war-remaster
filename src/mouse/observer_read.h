#ifndef HOGS_OBSERVER_READ_H
#define HOGS_OBSERVER_READ_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

enum {
    OBS_STATE_OK = 1u << 0,
    OBS_BATTLE_OK = 1u << 1,
    OBS_WEAPON_OK = 1u << 2,
    OBS_CONTROLLER_OK = 1u << 3,
    OBS_CAMERA_OK = 1u << 4,
    OBS_MODE_OK = 1u << 5,
    OBS_CAMERA_FIELDS_OK = 1u << 6,
    OBS_ANGLES_OK = 1u << 7,
    OBS_USER_OK = 1u << 8,
    OBS_TARGET54_PTR_OK = 1u << 9,
    OBS_TARGET54_TYPE_OK = 1u << 10,
    OBS_TARGET54_HEADING_OK = 1u << 11,
    OBS_TARGETB0_PTR_OK = 1u << 12,
    OBS_TARGETB0_TYPE_OK = 1u << 13,
    OBS_TARGETB0_HEADING_OK = 1u << 14,
    OBS_ACTIVE_PIG_PTR_OK = 1u << 15,
    OBS_ACTIVE_PIG_TYPE_OK = 1u << 16,
    OBS_ACTIVE_PIG_HEADING_OK = 1u << 17,
    OBS_ACTIVE_PIG_TRANSFORM_OK = 1u << 18,
    OBS_ACTIVE_PIG_POSITION_OK = 1u << 19
};

typedef struct {
    uint32_t flags;
    int user_readable;
    uint32_t state_ptr, camera_ptr;
    uint32_t battle, weapon, controller, camera_mode;
    int16_t cam_s16_76;
    uint16_t cam_u16_78, cam_u16_b4;
    int32_t angle_88, angle_8c, angle_90;
    uint32_t target54_ptr, target54_type, targetb0_ptr, targetb0_type;
    int32_t target54_heading, targetb0_heading;
    uint32_t active_pig_ptr, active_pig_type, active_pig_transform_ptr;
    int32_t active_pig_heading;
    float active_pig_x, active_pig_y, active_pig_z;
    int controller_matches_user;
} HogsObserverSnapshot;

typedef struct {
    uint8_t dik_valid, key_valid_mask, key_held_mask;
    uint16_t dik_codes[6]; /* up, down, left, right, L1, Cross */
} HogsObserverInputKeys;

typedef struct {
    uint32_t tick;
    uintptr_t user_ptr;
    HogsObserverSnapshot snapshot;
    uint8_t buttons_held, buttons_known, raw_held, legacy_held, poll_held;
    uint16_t mouse_mask;
    uint8_t prior_ok;
    HogsObserverInputKeys input_keys;
} HogsObserverRow;

static int hogs_observer_format_row(char *out, size_t capacity, const HogsObserverRow *r) {
    if (!out || !capacity || !r) return -1;
    const HogsObserverSnapshot *s = &r->snapshot;
    int n = snprintf(out, capacity,
        "%lu,%p,%d,%08lx,%d,%08lx,%d,%08lx,%d,%08lx,%d,%d,"
        "%08lx,%d,%08lx,%d,%ld,%d,%08lx,%d,%.7g,%.7g,%.7g,%d,"
        "%08lx,%d,%08lx,%d,%d,%u,%u,%d,%ld,%ld,%ld,%d,"
        "%08lx,%d,%08lx,%d,%ld,%d,%08lx,%d,%08lx,%d,%ld,%d,"
        "%02x,%02x,%02x,%02x,%02x,%04x,%u,%d,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\r\n",
        (unsigned long)r->tick, (void *)r->user_ptr, s->user_readable,
        (unsigned long)s->state_ptr, !!(s->flags & OBS_STATE_OK),
        (unsigned long)s->battle, !!(s->flags & OBS_BATTLE_OK),
        (unsigned long)s->weapon, !!(s->flags & OBS_WEAPON_OK),
        (unsigned long)s->controller, !!(s->flags & OBS_CONTROLLER_OK), s->controller_matches_user,
        (unsigned long)s->active_pig_ptr, !!(s->flags & OBS_ACTIVE_PIG_PTR_OK),
        (unsigned long)s->active_pig_type, !!(s->flags & OBS_ACTIVE_PIG_TYPE_OK),
        (long)s->active_pig_heading, !!(s->flags & OBS_ACTIVE_PIG_HEADING_OK),
        (unsigned long)s->active_pig_transform_ptr, !!(s->flags & OBS_ACTIVE_PIG_TRANSFORM_OK),
        (double)s->active_pig_x, (double)s->active_pig_y, (double)s->active_pig_z,
        !!(s->flags & OBS_ACTIVE_PIG_POSITION_OK),
        (unsigned long)s->camera_ptr, !!(s->flags & OBS_CAMERA_OK),
        (unsigned long)s->camera_mode, !!(s->flags & OBS_MODE_OK),
        (int)s->cam_s16_76, (unsigned)s->cam_u16_78, (unsigned)s->cam_u16_b4,
        !!(s->flags & OBS_CAMERA_FIELDS_OK),
        (long)s->angle_88, (long)s->angle_8c, (long)s->angle_90,
        !!(s->flags & OBS_ANGLES_OK), (unsigned long)s->target54_ptr,
        !!(s->flags & OBS_TARGET54_PTR_OK), (unsigned long)s->target54_type,
        !!(s->flags & OBS_TARGET54_TYPE_OK), (long)s->target54_heading,
        !!(s->flags & OBS_TARGET54_HEADING_OK), (unsigned long)s->targetb0_ptr,
        !!(s->flags & OBS_TARGETB0_PTR_OK), (unsigned long)s->targetb0_type,
        !!(s->flags & OBS_TARGETB0_TYPE_OK), (long)s->targetb0_heading,
        !!(s->flags & OBS_TARGETB0_HEADING_OK), (unsigned)r->buttons_held, (unsigned)r->buttons_known,
        (unsigned)r->raw_held, (unsigned)r->legacy_held, (unsigned)r->poll_held,
        (unsigned)r->mouse_mask, (unsigned)r->prior_ok, (unsigned)r->input_keys.dik_valid,
        (unsigned)r->input_keys.key_valid_mask, (unsigned)r->input_keys.dik_codes[0],
        (unsigned)r->input_keys.dik_codes[1], (unsigned)r->input_keys.dik_codes[2],
        (unsigned)r->input_keys.dik_codes[3], (unsigned)r->input_keys.dik_codes[4],
        (unsigned)r->input_keys.dik_codes[5], (unsigned)r->input_keys.key_held_mask);
    return n;
}

static int hogs_observer_readable(uintptr_t address, size_t bytes) {
    if (!address || !bytes || bytes > UINTPTR_MAX - address) return 0;
    uintptr_t end = address + bytes, cur = address;
    while (cur < end) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery((const void *)cur, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
            (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return 0;
        DWORD p = mbi.Protect & 0xff;
        if (p != PAGE_READONLY && p != PAGE_READWRITE && p != PAGE_WRITECOPY &&
            p != PAGE_EXECUTE_READ && p != PAGE_EXECUTE_READWRITE && p != PAGE_EXECUTE_WRITECOPY) return 0;
        uintptr_t base = (uintptr_t)mbi.BaseAddress;
        if (mbi.RegionSize > UINTPTR_MAX - base) return 0;
        uintptr_t region_end = base + mbi.RegionSize;
        if (cur < base || cur >= region_end) return 0;
        cur = region_end < end ? region_end : end;
    }
    return 1;
}

static int hogs_observer_read(uintptr_t address, void *out, size_t bytes) {
    if (!out || !hogs_observer_readable(address, bytes)) return 0;
    memcpy(out, (const void *)address, bytes);
    return 1;
}

static int hogs_observer_read_at(uint32_t base, uint32_t offset, void *out, size_t bytes) {
    if (!base || offset > UINT32_MAX - base) return 0;
    return hogs_observer_read((uintptr_t)(base + offset), out, bytes);
}

/* Reads only the requested documented offsets. Invalid pointers leave their
   value zero and their OBS_* validity bit clear; no blind dereference occurs. */
static void hogs_observer_capture(uintptr_t state_slot, uintptr_t camera_slot,
                                  uintptr_t user_ptr, HogsObserverSnapshot *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->user_readable = hogs_observer_readable(user_ptr, 0x128);
    if (out->user_readable) out->flags |= OBS_USER_OK;
    if (hogs_observer_read(state_slot, &out->state_ptr, sizeof(out->state_ptr)) && out->state_ptr &&
        hogs_observer_readable((uintptr_t)out->state_ptr, 0x514)) {
        out->flags |= OBS_STATE_OK;
        if (hogs_observer_read_at(out->state_ptr, 0x510, &out->battle, sizeof(out->battle))) out->flags |= OBS_BATTLE_OK;
        if (hogs_observer_read_at(out->state_ptr, 0x458, &out->weapon, sizeof(out->weapon))) out->flags |= OBS_WEAPON_OK;
        if (hogs_observer_read_at(out->state_ptr, 0x4fc, &out->controller, sizeof(out->controller))) {
            out->flags |= OBS_CONTROLLER_OK;
            out->controller_matches_user = out->controller == (uint32_t)user_ptr;
            if (out->controller && hogs_observer_read_at(out->controller, 0x10,
                    &out->active_pig_ptr, sizeof(out->active_pig_ptr)) &&
                out->active_pig_ptr && hogs_observer_readable((uintptr_t)out->active_pig_ptr, 0x178)) {
                out->flags |= OBS_ACTIVE_PIG_PTR_OK;
                if (hogs_observer_read_at(out->active_pig_ptr, 0x38,
                        &out->active_pig_type, sizeof(out->active_pig_type))) {
                    out->flags |= OBS_ACTIVE_PIG_TYPE_OK;
                    if (out->active_pig_type == 0x1357u) {
                        if (hogs_observer_read_at(out->active_pig_ptr, 0x174,
                                &out->active_pig_heading, sizeof(out->active_pig_heading)))
                            out->flags |= OBS_ACTIVE_PIG_HEADING_OK;
                        if (hogs_observer_read_at(out->active_pig_ptr, 0x18,
                                &out->active_pig_transform_ptr, sizeof(out->active_pig_transform_ptr)) &&
                            out->active_pig_transform_ptr &&
                            hogs_observer_readable((uintptr_t)out->active_pig_transform_ptr, 0x10)) {
                            out->flags |= OBS_ACTIVE_PIG_TRANSFORM_OK;
                            if (hogs_observer_read_at(out->active_pig_transform_ptr, 0x04,
                                    &out->active_pig_x, sizeof(float)) &&
                                hogs_observer_read_at(out->active_pig_transform_ptr, 0x08,
                                    &out->active_pig_y, sizeof(float)) &&
                                hogs_observer_read_at(out->active_pig_transform_ptr, 0x0c,
                                    &out->active_pig_z, sizeof(float)))
                                out->flags |= OBS_ACTIVE_PIG_POSITION_OK;
                        }
                    }
                }
            }
        }
    }
    if (hogs_observer_read(camera_slot, &out->camera_ptr, sizeof(out->camera_ptr)) && out->camera_ptr &&
        hogs_observer_readable((uintptr_t)out->camera_ptr, 0xb6)) {
        out->flags |= OBS_CAMERA_OK;
        if (hogs_observer_read_at(out->camera_ptr, 0x84, &out->camera_mode, sizeof(out->camera_mode))) out->flags |= OBS_MODE_OK;
        if (hogs_observer_read_at(out->camera_ptr, 0x76, &out->cam_s16_76, sizeof(out->cam_s16_76)) &&
            hogs_observer_read_at(out->camera_ptr, 0x78, &out->cam_u16_78, sizeof(out->cam_u16_78)) &&
            hogs_observer_read_at(out->camera_ptr, 0xb4, &out->cam_u16_b4, sizeof(out->cam_u16_b4)))
            out->flags |= OBS_CAMERA_FIELDS_OK;
        if (hogs_observer_read_at(out->camera_ptr, 0x88, &out->angle_88, sizeof(out->angle_88)) &&
            hogs_observer_read_at(out->camera_ptr, 0x8c, &out->angle_8c, sizeof(out->angle_8c)) &&
            hogs_observer_read_at(out->camera_ptr, 0x90, &out->angle_90, sizeof(out->angle_90)))
            out->flags |= OBS_ANGLES_OK;
        if (hogs_observer_read_at(out->camera_ptr, 0x54, &out->target54_ptr, sizeof(out->target54_ptr)) &&
            out->target54_ptr && hogs_observer_readable((uintptr_t)out->target54_ptr, 0x178)) {
            out->flags |= OBS_TARGET54_PTR_OK;
            if (hogs_observer_read_at(out->target54_ptr, 0x38, &out->target54_type, sizeof(out->target54_type)))
                out->flags |= OBS_TARGET54_TYPE_OK;
            if ((out->flags & OBS_TARGET54_TYPE_OK) && out->target54_type == 0x1357u &&
                hogs_observer_read_at(out->target54_ptr, 0x174, &out->target54_heading, sizeof(out->target54_heading)))
                out->flags |= OBS_TARGET54_HEADING_OK;
        }
        if ((out->flags & OBS_MODE_OK) && out->camera_mode == 2 &&
            hogs_observer_read_at(out->camera_ptr, 0xb0, &out->targetb0_ptr, sizeof(out->targetb0_ptr)) &&
            out->targetb0_ptr && hogs_observer_readable((uintptr_t)out->targetb0_ptr, 0x178)) {
            out->flags |= OBS_TARGETB0_PTR_OK;
            if (hogs_observer_read_at(out->targetb0_ptr, 0x38, &out->targetb0_type, sizeof(out->targetb0_type)))
                out->flags |= OBS_TARGETB0_TYPE_OK;
            if ((out->flags & OBS_TARGETB0_TYPE_OK) && out->targetb0_type == 0x1357u &&
                hogs_observer_read_at(out->targetb0_ptr, 0x174, &out->targetb0_heading, sizeof(out->targetb0_heading)))
                out->flags |= OBS_TARGETB0_HEADING_OK;
        }
    }
}

static void hogs_observer_capture_keys(uintptr_t dik_ptr, uintptr_t user_ptr,
                                       int prior_ok, HogsObserverInputKeys *out) {
    static const uint8_t indices[6] = {0, 1, 2, 3, 8, 5}; /* HogsFix PlayStation bit order */
    uint16_t bindings[16] = {0};
    uint8_t dik[256] = {0};
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (user_ptr <= UINTPTR_MAX - 0x108 &&
        hogs_observer_read(user_ptr + 0x108, bindings, sizeof(bindings))) {
        for (unsigned i = 0; i < 6; ++i) {
            uint16_t code = bindings[indices[i]];
            out->dik_codes[i] = code;
            if (code > 0 && code < 256) out->key_valid_mask |= (uint8_t)(1u << i);
        }
    }
    if (prior_ok && hogs_observer_read(dik_ptr, dik, sizeof(dik))) {
        out->dik_valid = 1;
        for (unsigned i = 0; i < 6; ++i)
            if ((out->key_valid_mask & (1u << i)) && (dik[out->dik_codes[i]] & 0x80))
                out->key_held_mask |= (uint8_t)(1u << i);
    }
}

#endif
