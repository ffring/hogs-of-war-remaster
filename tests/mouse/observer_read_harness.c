#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "observer_read.h"
#include "observer_buffer.h"

#define CHECK(cond, text) do { if (!(cond)) { fprintf(stderr, "FAIL %s\n", text); return 1; } } while (0)

static unsigned csv_fields(const char *s) {
    unsigned n = 1;
    for (; *s; ++s) if (*s == ',') ++n;
    return n;
}

typedef struct { char data[32768]; size_t used; unsigned calls; int fail; } TestSink;
static int test_sink(void *context, const char *bytes, size_t count) {
    TestSink *sink = (TestSink *)context;
    if (sink->fail || count > sizeof(sink->data) - sink->used) return 0;
    memcpy(sink->data + sink->used, bytes, count);
    sink->used += count; sink->calls++;
    return 1;
}
static unsigned line_count(const char *bytes, size_t count) {
    unsigned lines = 0;
    for (size_t i = 0; i < count; ++i) if (bytes[i] == '\n') lines++;
    return lines;
}
static int test_observer_batch_and_file_api(void) {
    HogsObserverBatch batch = {0}; TestSink sink = {{0},0,0,0};
    const char *a = "a,1\r\n", *b = "row-two,larger\r\n";
    CHECK(hogs_observer_batch_append(&batch, a, strlen(a)), "append first compact row");
    CHECK(hogs_observer_batch_append(&batch, b, strlen(b)), "append second variable-width row");
    CHECK(batch.rows == 2 && batch.used == strlen(a) + strlen(b), "compact byte accounting");
    CHECK(hogs_observer_batch_drain(&batch, test_sink, &sink), "drain two compact rows");
    CHECK(sink.used == strlen(a) + strlen(b) && !memcmp(sink.data, "a,1\r\nrow-two,larger\r\n", sink.used) &&
          line_count(sink.data, sink.used) == 2 && !memchr(sink.data, '\0', sink.used),
          "two-row exact bytes, NUL-free and EOF row count");

    char row[80], expected[8192]; size_t expected_used = 0;
    for (unsigned i = 0; i < HOGS_OBSERVER_BATCH_ROWS; ++i) {
        int n = snprintf(row, sizeof(row), "tick-%02u,value-%u\r\n", i, i * 17u);
        CHECK(n > 0 && (size_t)n < sizeof(row), "format variable row fixture");
        CHECK(hogs_observer_batch_append(&batch, row, (size_t)n), "append 64 variable rows");
        memcpy(expected + expected_used, row, (size_t)n); expected_used += (size_t)n;
    }
    CHECK(batch.rows == HOGS_OBSERVER_BATCH_ROWS, "64-row batch count");
    CHECK(hogs_observer_batch_drain(&batch, test_sink, &sink), "drain full 64-row batch");
    CHECK(sink.used == strlen(a) + strlen(b) + expected_used &&
          !memcmp(sink.data + strlen(a) + strlen(b), expected, expected_used) &&
          line_count(expected, expected_used) == HOGS_OBSERVER_BATCH_ROWS &&
          !memchr(sink.data, '\0', sink.used), "64-row exact stream, EOF count, no padding/NUL");

    const char *last = "tail,one\r\ntail,two-two\r\ntail,three\r\n";
    CHECK(hogs_observer_batch_append(&batch, "tail,one\r\n", strlen("tail,one\r\n")) &&
          hogs_observer_batch_append(&batch, "tail,two-two\r\n", strlen("tail,two-two\r\n")) &&
          hogs_observer_batch_append(&batch, "tail,three\r\n", strlen("tail,three\r\n")), "append final partial batch");
    sink.fail = 1;
    CHECK(!hogs_observer_batch_drain(&batch, test_sink, &sink) && batch.rows == 3 && batch.used == strlen(last),
          "failed sink retains final partial batch");
    sink.fail = 0; CHECK(hogs_observer_batch_drain(&batch, test_sink, &sink), "retry final partial batch");
    CHECK(!memcmp(sink.data + sink.used - strlen(last), last, strlen(last)) && batch.rows == 0 && batch.used == 0,
          "last partial batch exact bytes and cleared after drain");
    CHECK(!hogs_observer_batch_append(&batch, "x\0y", 3), "embedded NUL row rejected");

    char filename[64], path[MAX_PATH];
    CHECK(hogs_observer_make_filename(filename, sizeof(filename), GetCurrentProcessId(), GetTickCount(), 31) > 0,
          "bounded per-process observer filename");
    DWORD pn = GetTempPathA(MAX_PATH, path);
    CHECK(pn && pn < MAX_PATH && pn + strlen(filename) < MAX_PATH, "temporary API test path");
    lstrcatA(path, filename);
    HANDLE first = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, NULL);
    CHECK(first != INVALID_HANDLE_VALUE, "CREATE_NEW opens a new observer output file");
    CloseHandle(first);
    SetLastError(ERROR_SUCCESS);
    HANDLE second = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, NULL);
    CHECK(second == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_EXISTS,
          "CREATE_NEW refuses to overwrite prior observer output");
    CHECK(DeleteFileA(path), "temporary observer file cleanup");
    return 0;
}

int main(void) {
    CHECK(test_observer_batch_and_file_api() == 0, "observer batch/file API test");
    puts("PASS compact variable-width batch writes, full/partial drains, retry, and CREATE_NEW non-overwrite");
    SYSTEM_INFO si; GetSystemInfo(&si);
    SIZE_T page = si.dwPageSize;
    unsigned char *mem = (unsigned char *)VirtualAlloc(NULL, page * 8, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(mem != NULL, "fixture allocation");
    uint32_t *state_slot = (uint32_t *)(mem + page * 7);
    uint32_t *camera_slot = state_slot + 1;
    unsigned char *state = mem + page + 0x100;
    unsigned char *camera = mem + page * 2 + 0x100;
    unsigned char *target54 = mem + page * 3 + 0x100;
    uintptr_t user = (uintptr_t)(mem + page * 4 + 0x100);
    unsigned char *targetb0 = mem + page * 5 + 0x100;
    unsigned char *active_pig = mem + page * 5 + 0x300;
    unsigned char *transform = mem + page * 5 + 0x500;
    unsigned char *dik = mem + page * 6 + 0x100;
    *state_slot = (uint32_t)(uintptr_t)state; *camera_slot = (uint32_t)(uintptr_t)camera;
    *(uint32_t *)(state + 0x510) = 1; *(uint32_t *)(state + 0x458) = 7;
    *(uint32_t *)(state + 0x4fc) = (uint32_t)user;
    *(uint32_t *)(camera + 0x84) = 0; *(int16_t *)(camera + 0x76) = -7;
    *(uint16_t *)(camera + 0x78) = 42; *(uint16_t *)(camera + 0xb4) = 9;
    *(int32_t *)(camera + 0x88) = 0x12345678; *(int32_t *)(camera + 0x8c) = -8192; *(int32_t *)(camera + 0x90) = 4095;
    *(uint32_t *)(camera + 0x54) = (uint32_t)(uintptr_t)target54;
    *(uint32_t *)(camera + 0xb0) = (uint32_t)(uintptr_t)targetb0;
    *(uint32_t *)(target54 + 0x38) = 0x1357; *(int32_t *)(target54 + 0x174) = -123456;
    *(uint32_t *)(targetb0 + 0x38) = 0x1357; *(int32_t *)(targetb0 + 0x174) = 901;
    *(uint32_t *)((unsigned char *)user + 0x10) = (uint32_t)(uintptr_t)active_pig;
    *(uint32_t *)(active_pig + 0x38) = 0x1357; *(int32_t *)(active_pig + 0x174) = -2048;
    *(uint32_t *)(active_pig + 0x18) = (uint32_t)(uintptr_t)transform;
    *(float *)(transform + 0x04) = 123.5f; *(float *)(transform + 0x08) = -42.25f; *(float *)(transform + 0x0c) = 0.75f;
    uint16_t *bindings = (uint16_t *)(user + 0x108);
    bindings[0]=0x11; bindings[1]=0x12; bindings[2]=0x13; bindings[3]=0x14; bindings[8]=0x18; bindings[5]=0x15;
    dik[0x11]=0x80; dik[0x15]=0x80;

    HogsObserverSnapshot s;
    hogs_observer_capture((uintptr_t)state_slot, (uintptr_t)camera_slot, user, &s);
    CHECK(s.flags == 0xf8fffu, "readable state/camera/player/active pig and primary target fields");
    CHECK(s.controller_matches_user && s.weapon == 7 && s.battle == 1 && s.camera_mode == 0, "field values and controller match");
    CHECK(s.target54_ptr == (uint32_t)(uintptr_t)target54 && s.target54_type == 0x1357 && s.target54_heading == -123456,
          "camera+54 pig target type and signed heading");
    CHECK(s.angle_88 == 0x12345678 && s.angle_8c == -8192 && s.angle_90 == 4095,
          "camera angle triplet read as signed 32-bit integers");
    CHECK(s.active_pig_ptr == (uint32_t)(uintptr_t)active_pig && s.active_pig_type == 0x1357 &&
          s.active_pig_heading == -2048 && s.active_pig_transform_ptr == (uint32_t)(uintptr_t)transform &&
          (s.flags & OBS_ACTIVE_PIG_POSITION_OK) && s.active_pig_x == 123.5f && s.active_pig_y == -42.25f && s.active_pig_z == 0.75f,
          "active pig pointer, signed heading, and XYZ transform position");
    CHECK(!(s.flags & OBS_TARGETB0_PTR_OK), "alternative target only sampled in mode 2");
    *(uint32_t *)(camera + 0x84) = 2;
    hogs_observer_capture((uintptr_t)state_slot, (uintptr_t)camera_slot, user, &s);
    CHECK((s.flags & 0xfffffu) == 0xfffffu && s.targetb0_ptr == (uint32_t)(uintptr_t)targetb0 &&
          s.targetb0_type == 0x1357 && s.targetb0_heading == 901, "mode-2 alternative pig target fields");
    *(uint32_t *)(target54 + 0x38) = 3;
    hogs_observer_capture((uintptr_t)state_slot, (uintptr_t)camera_slot, user, &s);
    CHECK((s.flags & OBS_TARGET54_TYPE_OK) && !(s.flags & OBS_TARGET54_HEADING_OK),
          "non-pig camera target is not interpreted as a pig heading");
    *(uint32_t *)(target54 + 0x38) = 0x1357;

    HogsObserverInputKeys keys;
    hogs_observer_capture_keys((uintptr_t)dik, user, 1, &keys);
    CHECK(keys.dik_valid && keys.key_valid_mask == 0x3fu && keys.key_held_mask == 0x21u &&
          keys.dik_codes[4] == 0x18 && keys.dik_codes[5] == 0x15, "bound DIK key state read");
    hogs_observer_capture_keys((uintptr_t)dik, user, 0, &keys);
    CHECK(!keys.dik_valid && keys.key_valid_mask == 0x3fu && keys.key_held_mask == 0, "failed prior sample is not reported as pressed");
    HogsObserverRow row; memset(&row, 0, sizeof(row)); row.tick = 1234; row.user_ptr = user; row.snapshot = s;
    row.buttons_held = 3; row.buttons_known = 3; row.raw_held = 2; row.mouse_mask = 0x0120; row.prior_ok = 1;
    row.input_keys.dik_valid=1; row.input_keys.key_valid_mask=0x3f; row.input_keys.key_held_mask=0x21;
    row.input_keys.dik_codes[0]=0x11; row.input_keys.dik_codes[1]=0x12; row.input_keys.dik_codes[2]=0x13;
    row.input_keys.dik_codes[3]=0x14; row.input_keys.dik_codes[4]=0x18; row.input_keys.dik_codes[5]=0x15;
    char csv[512]; int n = hogs_observer_format_row(csv, sizeof(csv), &row);
    CHECK(n > 0 && n < (int)sizeof(csv) && csv_fields(csv) == 64, "bounded 64-column CSV serialization");
    CHECK(strstr(csv, "305419896,-8192,4095,1,") != NULL &&
          strstr(csv, ",0120,1,1,3f,11,12,13,14,18,15,21\r\n") != NULL,
          "serialized signed camera angles and native key snapshot");
    char short_buf[8]; CHECK(hogs_observer_format_row(short_buf, sizeof(short_buf), &row) >= (int)sizeof(short_buf), "truncated serialization is detectable");
    puts("PASS readable memory, typed camera targets, active-pig transform, bound keys, 64-column CSV and truncation detection");

    *(uint32_t *)(state + 0x4fc) = (uint32_t)user + 4;
    hogs_observer_capture((uintptr_t)state_slot, (uintptr_t)camera_slot, user, &s);
    CHECK((s.flags & OBS_CONTROLLER_OK) && !s.controller_matches_user, "controller mismatch recorded");
    puts("PASS controller mismatch remains explicit");

    *state_slot = 1; hogs_observer_capture((uintptr_t)state_slot, (uintptr_t)camera_slot, user, &s);
    CHECK(!(s.flags & OBS_STATE_OK) && !(s.flags & OBS_BATTLE_OK), "invalid state pointer rejected");
    *state_slot = (uint32_t)(uintptr_t)state;
    *camera_slot = 1; hogs_observer_capture((uintptr_t)state_slot, (uintptr_t)camera_slot, user, &s);
    CHECK(!(s.flags & OBS_CAMERA_OK) && !(s.flags & OBS_ANGLES_OK), "invalid camera pointer rejected");
    puts("PASS invalid state/camera pointers rejected without dereference");

    DWORD old = 0; unsigned char *guard_start = mem + page * 2;
    CHECK(VirtualProtect(guard_start, page, PAGE_NOACCESS, &old), "protect guard page");
    *camera_slot = (uint32_t)(uintptr_t)camera;
    *state_slot = (uint32_t)(uintptr_t)(guard_start - 0x500);
    hogs_observer_capture((uintptr_t)state_slot, (uintptr_t)camera_slot, user, &s);
    CHECK(!(s.flags & OBS_STATE_OK) && !(s.flags & OBS_BATTLE_OK), "cross-page unreadable state rejected");
    CHECK(VirtualProtect(guard_start, page, old, &old), "restore guard page");
    puts("PASS unreadable cross-page state range rejected");
    VirtualFree(mem, 0, MEM_RELEASE);
    return 0;
}
