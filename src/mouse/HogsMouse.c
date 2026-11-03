#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mouse_logic.h"
#include "message_pump_logic.h"
#include "poll_logic.h"
#include "button_state.h"
#include "observer_read.h"
#include "observer_buffer.h"

/* Narrow compatibility target: Steam/GOG-compatible November 2000 warhogs_.exe,
   loaded at 0x400000. The call address is the game's preferred VA from HogsFix. */
#define EXE_TIMESTAMP 0x3a069f75u
#define EXE_IMAGE_SIZE 0x15e000u
#define EXE_PREFERRED_BASE 0x400000u
#define KB_CALL_VA 0x45d72du
#define GAME_HWND_VA 0x520860u

typedef void (*KB_TARGET)(void); /* raw HogsFix naked stub: stack arg buf, EDI=user */
static HINSTANCE g_self;
static BOOL (WINAPI *g_real_peek)(LPMSG, HWND, UINT, UINT, UINT);
static int (WINAPI *g_real_get)(LPMSG, HWND, UINT, UINT);
static KB_TARGET g_prior_kb;
static HWND g_hwnd;
static DWORD g_game_thread;
static int g_installed, g_raw_registered, g_enabled = 1, g_cfg_loaded, g_armed, g_f6_down, g_focused;
static int g_initializing, g_permanent_failure;
static int g_sensitivity = 4, g_invert_y, g_require_rmb = 1;
static HMState g_mouse;
static HMState g_legacy_mouse;
static HMPollState g_poll_mouse;
static HMButtonState g_button_state;
static int g_aim_source;
static DWORD g_last_game_input_ms;
static DWORD g_raw_event_ms, g_legacy_event_ms;
static LONG g_raw_seen, g_legacy_active, g_legacy_baseline;
static SHORT g_legacy_x, g_legacy_y;
static LONG g_raw_registration_matches;
static LONG g_raw_registration_verified;
static DWORD g_raw_registration_check_at;
/* Chase-camera observer: bounded, read-only state sampling. These fields do
   not participate in control decisions or modify game memory. */
static uint16_t g_observer_mouse_mask;
#ifndef HOGSMOUSE_TEST
enum { OBS_MAX_ROWS = 4096 };
static HANDLE g_observer_file = INVALID_HANDLE_VALUE;
static HogsObserverBatch g_observer_batch;
static DWORD g_observer_last_flush;
static DWORD g_observer_rows;
static int g_observer_disabled;
#endif
/* Diagnostic-only counters for one human battle run. All increments/exchanges
   are atomic because a mismatched callback thread is itself a signal to log. */
static volatile LONG d_peek, d_get, d_callback, d_callback_thread, d_callback_ui, d_callback_ok0;
static volatile LONG d_wm_input, d_gate_disabled, d_gate_unfocused, d_gate_not_foreground;
static volatile LONG d_gate_no_callback, d_gate_stale_callback, d_gate_ui, d_raw_read_fail;
static volatile LONG d_nonmouse, d_absolute, d_disarmed, d_no_rmb, d_zero_motion;
static volatile LONG d_relative_motion, d_button_events, d_merge_added, d_merge_invalid_bind;
static volatile LONG d_legacy_events, d_legacy_ignored_raw, d_rebinds, d_rebind_fail;
static volatile LONG d_poll_sample, d_poll_motion, d_poll_buttons, d_poll_warp, d_poll_fail, d_poll_chosen;
static volatile LONG d_poll_x, d_poll_y, d_poll_held;
static volatile LONG d_raw_l_down, d_raw_l_up, d_raw_r_down, d_raw_r_up;
static volatile LONG d_legacy_l_down, d_legacy_l_up, d_legacy_r_down, d_legacy_r_up;
static volatile LONG d_aim_up, d_aim_down, d_aim_left, d_aim_right, d_aim_l1, d_aim_rmb_gated;
static DWORD d_report_at;
#define DIAG_INC(name) InterlockedIncrement(&d_##name)
#ifndef HOGSMOUSE_TEST
static char g_log_path[MAX_PATH];
#endif
static void poll_control_state(void);
static BOOL WINAPI mouse_peek(LPMSG, HWND, UINT, UINT, UINT);
static int WINAPI mouse_get(LPMSG, HWND, UINT, UINT);
static void patch_iat(void **, void *);
static int keyboard_callsite_owned(void);
static int game_ui_active(void);
static int is_game_foreground(void);
static void reset_aim_motion(void);
static void record_button_event(uint8_t button, int down);
#ifdef HOGSMOUSE_TEST
static DWORD g_test_now;
static uint8_t g_test_buttons;
static LONG g_test_cursor_x, g_test_cursor_y;
static int g_test_cursor_valid = 1;
static int g_test_foreground = 1, g_test_ui, g_test_old_calls;
static int g_test_native_focus = 1;
static int g_test_raw_registration_match = 1;
static volatile LONG g_test_msg_mutations;
#endif
#include "mac_focus_gate.h"

static DWORD now_ms(void) {
#ifdef HOGSMOUSE_TEST
    return g_test_now;
#else
    return GetTickCount();
#endif
}

static int is_game_foreground(void) {
#ifdef HOGSMOUSE_TEST
    return g_test_foreground && hm_mac_focus_gate();
#else
    return g_hwnd && GetForegroundWindow() == g_hwnd && hm_mac_focus_gate();
#endif
}

static void log_line(const char *s) {
#ifdef HOGSMOUSE_TEST
    (void)s;
    return;
#else
    if (!g_log_path[0]) {
        DWORD n = GetModuleFileNameA(g_self, g_log_path, MAX_PATH);
        while (n && g_log_path[n - 1] != '\\') n--;
        if (n < MAX_PATH - 15) lstrcpyA(g_log_path + n, "HogsMouse.log");
        else return;
    }
    HANDLE f = CreateFileA(g_log_path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD written;
    WriteFile(f, s, (DWORD)lstrlenA(s), &written, NULL);
    WriteFile(f, "\r\n", 2, &written, NULL);
    CloseHandle(f);
#endif
}

#ifndef HOGSMOUSE_TEST
static int observer_write_all(HANDLE file, const void *bytes, DWORD count) {
    const char *p = (const char *)bytes;
    while (count) {
        DWORD written = 0;
        if (!WriteFile(file, p, count, &written, NULL) || !written) return 0;
        p += written; count -= written;
    }
    return 1;
}

static int observer_file_sink(void *context, const char *bytes, size_t count) {
    HANDLE file = (HANDLE)context;
    return count <= 0xffffffffu && observer_write_all(file, bytes, (DWORD)count) && FlushFileBuffers(file);
}

static int observer_open(void) {
    if (g_observer_file != INVALID_HANDLE_VALUE) return 1;
    if (g_observer_disabled) return 0;
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(g_self, path, MAX_PATH);
    if (!n || n >= MAX_PATH) { g_observer_disabled = 1; return 0; }
    while (n && path[n - 1] != '\\') --n;
    if (!n) { g_observer_disabled = 1; return 0; }
    char name[64];
    for (DWORD attempt = 0; attempt < 32; ++attempt) {
        int name_length = hogs_observer_make_filename(name, sizeof(name), GetCurrentProcessId(),
                                                     GetTickCount(), attempt);
        if (name_length < 0 || n + (DWORD)name_length + 1 > MAX_PATH) { g_observer_disabled = 1; return 0; }
        memcpy(path + n, name, (size_t)name_length + 1);
        g_observer_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW,
                                      FILE_ATTRIBUTE_NORMAL, NULL);
        if (g_observer_file != INVALID_HANDLE_VALUE) break;
        DWORD error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) break;
    }
    if (g_observer_file == INVALID_HANDLE_VALUE) { g_observer_disabled = 1; return 0; }
    static const char header[] = "tick,user,user_ok,state,state_ok,battle,battle_ok,weapon,weapon_ok,controller,controller_ok,controller_matches_user,active_pig_ptr,active_pig_ptr_ok,active_pig_type,active_pig_type_ok,active_pig_heading_i32,active_pig_heading_ok,active_pig_transform_ptr,active_pig_transform_ptr_ok,active_pig_pos_x,active_pig_pos_y,active_pig_pos_z,active_pig_position_ok,camera,camera_ok,cam_mode,mode_ok,cam_s16_76,cam_u16_78,cam_u16_b4,cam_fields_ok,angle_88_i32,angle_8c_i32,angle_90_i32,angles_ok,target54_ptr,target54_ptr_ok,target54_type,target54_type_ok,target54_heading_i32,target54_heading_ok,targetb0_ptr,targetb0_ptr_ok,targetb0_type,targetb0_type_ok,targetb0_heading_i32,targetb0_heading_ok,buttons_held,buttons_known,raw_held,legacy_held,poll_held,mouse_mask,prior_ok,dik_valid,key_valid_mask,up_dik,down_dik,left_dik,right_dik,l1_dik,cross_dik,key_held_mask\r\n";
    if (!observer_write_all(g_observer_file, header, sizeof(header) - 1)) {
        CloseHandle(g_observer_file); g_observer_file = INVALID_HANDLE_VALUE; g_observer_disabled = 1; return 0;
    }
    g_observer_last_flush = GetTickCount();
    return 1;
}

static void observer_flush(void) {
    if (g_observer_file == INVALID_HANDLE_VALUE || !g_observer_batch.used) return;
    if (!hogs_observer_batch_drain(&g_observer_batch, observer_file_sink, g_observer_file)) {
        CloseHandle(g_observer_file); g_observer_file = INVALID_HANDLE_VALUE;
        g_observer_disabled = 1; hogs_observer_batch_clear(&g_observer_batch); return;
    }
    g_observer_last_flush = GetTickCount();
}

static void observer_tick(BYTE *user, BYTE prior_ok, const HogsObserverInputKeys *input_keys) {
    if (g_observer_disabled || g_observer_rows >= OBS_MAX_ROWS || !g_installed || !g_enabled ||
        !g_armed || !g_focused || GetCurrentThreadId() != g_game_thread ||
        !is_game_foreground() || game_ui_active() || !user) return;

    HogsObserverSnapshot s;
    hogs_observer_capture((uintptr_t)0x00537f24, (uintptr_t)0x00537f98,
                          (uintptr_t)user, &s);
    if (!(s.flags & OBS_USER_OK) || ((s.flags & OBS_CONTROLLER_OK) && !s.controller_matches_user)) return;

    if (!observer_open()) return;
    char row[HOGS_OBSERVER_ROW_BYTES];
    HogsObserverRow record;
    memset(&record, 0, sizeof(record));
    record.tick = GetTickCount(); record.user_ptr = (uintptr_t)user; record.snapshot = s;
    record.buttons_held = g_button_state.held; record.buttons_known = g_button_state.known;
    record.raw_held = g_mouse.held; record.legacy_held = g_legacy_mouse.held;
    record.poll_held = g_poll_mouse.mouse.held; record.mouse_mask = g_observer_mouse_mask;
    record.prior_ok = prior_ok;
    if (input_keys) record.input_keys = *input_keys;
    int n = hogs_observer_format_row(row, sizeof(row), &record);
    if (n <= 0 || n >= (int)sizeof(row)) { g_observer_disabled = 1; return; }
    if (!hogs_observer_batch_append(&g_observer_batch, row, (size_t)n)) {
        observer_flush();
        if (g_observer_file == INVALID_HANDLE_VALUE ||
            !hogs_observer_batch_append(&g_observer_batch, row, (size_t)n)) {
            g_observer_disabled = 1; return;
        }
    }
    g_observer_rows++;
    DWORD now = GetTickCount();
    if (g_observer_batch.rows >= HOGS_OBSERVER_BATCH_ROWS || (DWORD)(now - g_observer_last_flush) >= 2000) observer_flush();
}
#else
static void observer_tick(BYTE *user, BYTE prior_ok, const HogsObserverInputKeys *input_keys) { (void)user; (void)prior_ok; (void)input_keys; }
#endif

static void report_diagnostics(void) {
    DWORD now = now_ms();
    char line[2048];
    if (d_report_at && (int32_t)(now - d_report_at) < 0) return;
    d_report_at = now + 2000;
    RAWINPUTDEVICE regs[64]; UINT count = ARRAYSIZE(regs), found = 0; HWND raw_hwnd = NULL; DWORD raw_flags = 0;
    UINT nr = GetRegisteredRawInputDevices(regs, &count, sizeof(RAWINPUTDEVICE));
    if (nr != (UINT)-1) for (UINT i = 0; i < nr; ++i)
        if (regs[i].usUsagePage == 1 && regs[i].usUsage == 2) { found++; raw_hwnd = regs[i].hwndTarget; raw_flags = regs[i].dwFlags; }
    int written = snprintf(line, sizeof(line),
        "DIAG3s peek=%ld get=%ld cb=%ld cbOtherThread=%ld cbUI=%ld cbAL0=%ld raw=%ld gates(disabled=%ld focus=%ld foreground=%ld noCB=%ld stale=%ld UI=%ld) readFail=%ld nonmouse=%ld abs=%ld disarmed=%ld noRMB=%ld zero=%ld rel=%ld buttons=%ld rawFlags(L+%ld L-%ld R+%ld R-%ld) legacy=%ld legacyIgnoredRaw=%ld legacyFlags(L+%ld L-%ld R+%ld R-%ld) merge=%ld invalidBind=%ld aim(U=%ld D=%ld L=%ld R=%ld L1=%ld gated=%ld) rebind=%ld rebindFail=%ld poll(sample=%ld motion=%ld buttons=%ld warp=%ld fail=%ld chosen=%ld xy=%d,%d held=%02x) rawReg(local=%d found=%u target=%p flags=%08lx match=%ld) callsiteOwned=%d prior=%p subclassHook=%d state(en=%d focus=%d fg=%d macFocus=%d armed=%d rawAge=%lu rawSeen=%ld legacyActive=%ld heldRaw=%02x heldLegacy=%02x heldPoll=%02x buttonHeld=%02x buttonKnown=%02x waitRelease=%02x dxRaw=%d dyRaw=%d dxLegacy=%d dyLegacy=%d dxPoll=%d dyPoll=%d cbAge=%lu gameHwnd=%p winTid=%lu tid=%lu/%lu)",
        InterlockedExchange(&d_peek, 0), InterlockedExchange(&d_get, 0), InterlockedExchange(&d_callback, 0),
        InterlockedExchange(&d_callback_thread, 0), InterlockedExchange(&d_callback_ui, 0),
        InterlockedExchange(&d_callback_ok0, 0), InterlockedExchange(&d_wm_input, 0),
        InterlockedExchange(&d_gate_disabled, 0), InterlockedExchange(&d_gate_unfocused, 0),
        InterlockedExchange(&d_gate_not_foreground, 0), InterlockedExchange(&d_gate_no_callback, 0),
        InterlockedExchange(&d_gate_stale_callback, 0), InterlockedExchange(&d_gate_ui, 0),
        InterlockedExchange(&d_raw_read_fail, 0), InterlockedExchange(&d_nonmouse, 0),
        InterlockedExchange(&d_absolute, 0), InterlockedExchange(&d_disarmed, 0),
        InterlockedExchange(&d_no_rmb, 0), InterlockedExchange(&d_zero_motion, 0),
        InterlockedExchange(&d_relative_motion, 0), InterlockedExchange(&d_button_events, 0),
        InterlockedExchange(&d_raw_l_down, 0), InterlockedExchange(&d_raw_l_up, 0),
        InterlockedExchange(&d_raw_r_down, 0), InterlockedExchange(&d_raw_r_up, 0),
        InterlockedExchange(&d_legacy_events, 0), InterlockedExchange(&d_legacy_ignored_raw, 0),
        InterlockedExchange(&d_legacy_l_down, 0), InterlockedExchange(&d_legacy_l_up, 0),
        InterlockedExchange(&d_legacy_r_down, 0), InterlockedExchange(&d_legacy_r_up, 0),
        InterlockedExchange(&d_merge_added, 0), InterlockedExchange(&d_merge_invalid_bind, 0),
        InterlockedExchange(&d_aim_up, 0), InterlockedExchange(&d_aim_down, 0),
        InterlockedExchange(&d_aim_left, 0), InterlockedExchange(&d_aim_right, 0),
        InterlockedExchange(&d_aim_l1, 0), InterlockedExchange(&d_aim_rmb_gated, 0),
        InterlockedExchange(&d_rebinds, 0), InterlockedExchange(&d_rebind_fail, 0),
        InterlockedExchange(&d_poll_sample, 0), InterlockedExchange(&d_poll_motion, 0),
        InterlockedExchange(&d_poll_buttons, 0), InterlockedExchange(&d_poll_warp, 0),
        InterlockedExchange(&d_poll_fail, 0), InterlockedExchange(&d_poll_chosen, 0),
        (int)InterlockedCompareExchange(&d_poll_x, 0, 0),
        (int)InterlockedCompareExchange(&d_poll_y, 0, 0),
        (BYTE)InterlockedCompareExchange(&d_poll_held, 0, 0),
        g_raw_registered, found, raw_hwnd, raw_flags, g_raw_registration_matches,
        keyboard_callsite_owned(), (void *)g_prior_kb, 0,
        g_enabled, g_focused, is_game_foreground(), hm_mac_focus_status_code(), g_armed,
        g_raw_seen ? (unsigned long)(now - g_raw_event_ms) : 0xfffffffful, g_raw_seen, g_legacy_active,
        g_mouse.held, g_legacy_mouse.held, g_poll_mouse.mouse.held,
        g_button_state.held, g_button_state.known, g_button_state.require_event_release,
        g_mouse.dx, g_mouse.dy, g_legacy_mouse.dx, g_legacy_mouse.dy,
        g_poll_mouse.mouse.dx, g_poll_mouse.mouse.dy,
        g_last_game_input_ms ? (unsigned long)(now - g_last_game_input_ms) : 0xfffffffful,
        g_hwnd,
        (unsigned long)(g_hwnd ? GetWindowThreadProcessId(g_hwnd, NULL) : 0),
        (unsigned long)GetCurrentThreadId(), (unsigned long)g_game_thread);
    if (written < 0) line[0] = '\0';
    line[sizeof(line) - 1] = '\0';
    if (line[0]) log_line(line);
}

static void **find_iat(HMODULE module, const char *dll, const char *name) {
    BYTE *base = (BYTE *)module;
    if (!base) return NULL;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    IMAGE_NT_HEADERS32 *nt = (IMAGE_NT_HEADERS32 *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return NULL;
    DWORD import_rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!import_rva) return NULL;
    IMAGE_IMPORT_DESCRIPTOR *id = (IMAGE_IMPORT_DESCRIPTOR *)(base + import_rva);
    for (; id->Name; ++id) {
        if (lstrcmpiA((char *)(base + id->Name), dll)) continue;
        IMAGE_THUNK_DATA32 *orig = (IMAGE_THUNK_DATA32 *)(base + (id->OriginalFirstThunk ? id->OriginalFirstThunk : id->FirstThunk));
        DWORD *slots = (DWORD *)(base + id->FirstThunk);
        for (; orig->u1.AddressOfData; ++orig, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL32(orig->u1.Ordinal)) continue;
            IMAGE_IMPORT_BY_NAME *by_name = (IMAGE_IMPORT_BY_NAME *)(base + orig->u1.AddressOfData);
            if (!lstrcmpA((char *)by_name->Name, name)) return (void **)slots;
        }
    }
    return NULL;
}

static int module_contains(HMODULE module, const void *p) {
    if (!module || !p) return 0;
    BYTE *base = (BYTE *)module;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    IMAGE_NT_HEADERS32 *nt = (IMAGE_NT_HEADERS32 *)(base + dos->e_lfanew);
    uintptr_t start = (uintptr_t)base, addr = (uintptr_t)p;
    return nt->Signature == IMAGE_NT_SIGNATURE && addr >= start &&
           addr < start + nt->OptionalHeader.SizeOfImage;
}

static int module_contains_range(HMODULE module, const void *p, DWORD bytes) {
    BYTE *base = (BYTE *)module;
    if (!module_contains(module, p)) return 0;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS32 *nt = (IMAGE_NT_HEADERS32 *)(base + dos->e_lfanew);
    uintptr_t end = (uintptr_t)base + nt->OptionalHeader.SizeOfImage;
    return (uintptr_t)p <= end && bytes <= end - (uintptr_t)p;
}

static int supported_exe(DWORD *base_out) {
    BYTE *base = (BYTE *)GetModuleHandleA(NULL);
    if (!base || (DWORD)base != EXE_PREFERRED_BASE) return 0;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    IMAGE_NT_HEADERS32 *nt = (IMAGE_NT_HEADERS32 *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.TimeDateStamp != EXE_TIMESTAMP ||
        nt->OptionalHeader.SizeOfImage != EXE_IMAGE_SIZE || nt->OptionalHeader.ImageBase != EXE_PREFERRED_BASE) return 0;
    *base_out = (DWORD)base;
    return 1;
}

static BOOL register_mouse(HWND hwnd) {
    g_raw_registration_matches = 0;
    UINT count = 0;
    SetLastError(0);
    UINT got = GetRegisteredRawInputDevices(NULL, &count, sizeof(RAWINPUTDEVICE));
    if (got == (UINT)-1 && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        log_line("Raw Input enumeration failed; disabled safely."); return FALSE;
    }
    RAWINPUTDEVICE devices[64];
    if (count > ARRAYSIZE(devices)) { log_line("Too many Raw Input registrations; disabled safely."); return FALSE; }
    if (count) {
        UINT cap = count;
        got = GetRegisteredRawInputDevices(devices, &cap, sizeof(RAWINPUTDEVICE));
        if (got == (UINT)-1) { log_line("Raw Input enumeration failed; disabled safely."); return FALSE; }
        for (UINT i = 0; i < got; ++i) {
            if (devices[i].usUsagePage == 1 && devices[i].usUsage == 2) {
                log_line("Another mouse Raw Input registration exists; disabled safely.");
                return FALSE;
            }
        }
    }
    RAWINPUTDEVICE rid;
    rid.usUsagePage = 1; rid.usUsage = 2; rid.dwFlags = 0; rid.hwndTarget = hwnd;
    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) { log_line("RegisterRawInputDevices failed; disabled safely."); return FALSE; }
    g_raw_registered = 1;
    g_raw_registration_matches = 1;
    g_raw_registration_verified = 1;
    g_raw_registration_check_at = 0;
    return TRUE;
}

static void reset_aim_motion(void) {
    hm_clear(&g_mouse);
    hm_clear(&g_legacy_mouse);
    hm_poll_reset(&g_poll_mouse);
    g_legacy_baseline = 0;
    g_aim_source = HM_SOURCE_NONE;
}

static void record_button_event(uint8_t button, int down) {
    uint8_t before = g_button_state.held;
    hm_button_event(&g_button_state, button, down);
    if (button == HM_MOUSE_RIGHT && ((before ^ g_button_state.held) & HM_MOUSE_RIGHT))
        reset_aim_motion();
}

static void clear_and_disarm(const char *reason) {
    hm_button_disarm(&g_button_state);
    reset_aim_motion();
    g_raw_seen = g_legacy_active = g_legacy_baseline = 0;
    g_raw_event_ms = g_legacy_event_ms = 0;
    g_armed = 0;
    g_last_game_input_ms = 0;
    if (reason) log_line(reason);
}

static void unregister_mouse(void) {
    if (g_raw_registered) {
        RAWINPUTDEVICE regs[64]; UINT count = ARRAYSIZE(regs);
        UINT got = GetRegisteredRawInputDevices(regs, &count, sizeof(RAWINPUTDEVICE));
        int owned = 0;
        if (got != (UINT)-1) for (UINT i = 0; i < got; ++i)
            if (regs[i].usUsagePage == 1 && regs[i].usUsage == 2 && regs[i].hwndTarget == g_hwnd && regs[i].dwFlags == 0) owned = 1;
        if (owned) {
            RAWINPUTDEVICE rid;
            rid.usUsagePage = 1; rid.usUsage = 2; rid.dwFlags = RIDEV_REMOVE; rid.hwndTarget = NULL;
            RegisterRawInputDevices(&rid, 1, sizeof(rid));
        } else {
            log_line("Raw Input registration changed owner; left current registration untouched.");
        }
        g_raw_registered = 0;
        g_raw_registration_matches = 0;
        g_raw_registration_verified = 0;
        g_raw_registration_check_at = 0;
    }
}

static BYTE physical_buttons_down_mask(void) {
#ifdef HOGSMOUSE_TEST
    return g_test_buttons & (HM_MOUSE_LEFT | HM_MOUSE_RIGHT);
#else
    BYTE down = 0;
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) down |= HM_MOUSE_LEFT;
    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) down |= HM_MOUSE_RIGHT;
    return down;
#endif
}

static int sync_button_poll(void) {
    BYTE before = g_button_state.held;
    hm_button_poll(&g_button_state, physical_buttons_down_mask());
    if (before != g_button_state.held) DIAG_INC(poll_buttons);
    if ((before ^ g_button_state.held) & HM_MOUSE_RIGHT) reset_aim_motion();
    return hm_button_state_released(&g_button_state);
}

static int physical_buttons_up(void) { return sync_button_poll(); }

/* Detect ownership changes on the game thread, independently of mouse
   movement. A quiet mouse is normal; a previously verified registration
   being absent/replaced is a reason to disarm event-held controls. */
static void refresh_raw_registration(void) {
    if (!g_raw_registered || !g_raw_registration_verified) return;
    DWORD now = now_ms();
    if (g_raw_registration_check_at && (int32_t)(now - g_raw_registration_check_at) < 0) return;
    g_raw_registration_check_at = now + 100;
    int matches = 0;
#ifdef HOGSMOUSE_TEST
    matches = g_test_raw_registration_match;
#else
    RAWINPUTDEVICE regs[64]; UINT count = ARRAYSIZE(regs);
    UINT got = GetRegisteredRawInputDevices(regs, &count, sizeof(RAWINPUTDEVICE));
    if (got != (UINT)-1) {
        for (UINT i = 0; i < got; ++i) {
            if (regs[i].usUsagePage == 1 && regs[i].usUsage == 2 &&
                regs[i].hwndTarget == g_hwnd && regs[i].dwFlags == 0) {
                matches = 1;
                break;
            }
        }
    }
#endif
    if (g_raw_registration_matches && !matches) {
        g_raw_registration_matches = 0;
        clear_and_disarm("Raw Input registration was lost or replaced; held mouse state cleared; release required.");
    } else if (!g_raw_registration_matches && matches) {
        g_raw_registration_matches = 1;
        g_raw_registration_check_at = now + 100;
        log_line("Owned Raw Input registration returned; release both buttons before rearming.");
    }
}

static int poll_cursor_sample(LONG *x, LONG *y) {
#ifdef HOGSMOUSE_TEST
    if (!g_test_cursor_valid) return 0;
    *x = g_test_cursor_x; *y = g_test_cursor_y;
    return 1;
#else
    POINT p;
    if (!GetCursorPos(&p)) return 0;
    *x = p.x; *y = p.y;
    return 1;
#endif
}

static void sample_poll_mouse(DWORD now) {
    LONG x = 0, y = 0;
    int ok = poll_cursor_sample(&x, &y);
    sync_button_poll();
    int event = hm_poll_update(&g_poll_mouse, ok, x, y, 0, now,
                               g_armed && g_enabled && g_focused && is_game_foreground() && !game_ui_active(),
                               0, 128);
    int selected = hm_poll_choose_source(g_raw_registered && g_raw_registration_matches,
        g_raw_seen, g_raw_event_ms, g_legacy_active, g_legacy_event_ms,
        g_poll_mouse.sample_valid, now);
    /* Keep the poll baseline current while an event source owns motion, but
       discard its deltas so they cannot replay when Raw/legacy goes stale. */
    hm_poll_discard_if_unselected(&g_poll_mouse, selected);
    if (ok) {
        DIAG_INC(poll_sample);
        InterlockedExchange(&d_poll_x, x); InterlockedExchange(&d_poll_y, y);
        InterlockedExchange(&d_poll_held, g_button_state.held);
    } else DIAG_INC(poll_fail);
    if (event == HM_POLL_MOTION && selected == HM_SOURCE_POLL) DIAG_INC(poll_motion);
    else if (event == HM_POLL_WARP_REJECTED) DIAG_INC(poll_warp);
}

static int game_ui_active(void) {
#ifdef HOGSMOUSE_TEST
    return g_test_ui;
#else
    /* These globals come from the exact GOG PE layout validated before hooks install. */
    DWORD settings_ptr = *(DWORD *)0x520668;
    return *(DWORD *)0x51f120 != 0 ||
           (settings_ptr && *((BYTE *)settings_ptr + 0x479) != 0); /* frontend or debrief */
#endif
}

static int game_input_gate(HWND hwnd) {
    DWORD now = now_ms();
    if (g_permanent_failure) return 0;
    if (!g_enabled) { DIAG_INC(gate_disabled); return 0; }
    if (!g_focused || hwnd != g_hwnd) { DIAG_INC(gate_unfocused); return 0; }
    if (!is_game_foreground()) { DIAG_INC(gate_not_foreground); return 0; }
    if (!g_last_game_input_ms) { DIAG_INC(gate_no_callback); return 0; }
    if ((uint32_t)(now - g_last_game_input_ms) > 100) { DIAG_INC(gate_stale_callback); return 0; }
    if (game_ui_active()) { DIAG_INC(gate_ui); return 0; }
    return 1;
}

static void process_legacy_mouse(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    DIAG_INC(legacy_events);
    if (!g_installed || hwnd != g_hwnd || GetCurrentThreadId() != g_game_thread) return;
    int active = game_input_gate(hwnd);
    DWORD now = now_ms();
    int raw_fresh = hm_raw_recent(g_raw_registered && g_raw_registration_matches,
                                  g_raw_seen, g_raw_event_ms, now);
    /* Button transitions are independent of movement-source arbitration. The
       helper makes duplicate Raw/legacy edges idempotent, and a legacy UP is
       still useful if Raw Input stopped owning the process registration. */
    if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP) {
        if (msg == WM_LBUTTONDOWN) { if (active) record_button_event(HM_MOUSE_LEFT, 1); DIAG_INC(legacy_l_down); }
        else if (msg == WM_LBUTTONUP) { record_button_event(HM_MOUSE_LEFT, 0); DIAG_INC(legacy_l_up); }
        else if (msg == WM_RBUTTONDOWN) { if (active) record_button_event(HM_MOUSE_RIGHT, 1); else reset_aim_motion(); DIAG_INC(legacy_r_down); }
        else { record_button_event(HM_MOUSE_RIGHT, 0); DIAG_INC(legacy_r_up); }
        if (!g_armed && g_enabled && g_focused && is_game_foreground() && !game_ui_active() && hm_button_state_released(&g_button_state)) g_armed = 1;
        if (!active) return;
        if (raw_fresh) { DIAG_INC(legacy_ignored_raw); return; }
    } else if (raw_fresh) {
        DIAG_INC(legacy_ignored_raw);
        return;
    }
    if (!active) return;
    if (!g_legacy_active) {
        hm_clear(&g_legacy_mouse);
        hm_clear(&g_mouse);
        g_legacy_baseline = 0;
        g_legacy_active = 1;
    }
    g_legacy_event_ms = now;
    if (msg == WM_MOUSEMOVE) {
        record_button_event(HM_MOUSE_LEFT, (wp & MK_LBUTTON) != 0);
        record_button_event(HM_MOUSE_RIGHT, (wp & MK_RBUTTON) != 0);
        SHORT x = (SHORT)LOWORD(lp), y = (SHORT)HIWORD(lp);
        if (!g_legacy_baseline) {
            g_legacy_x = x; g_legacy_y = y; g_legacy_baseline = 1;
        } else {
            int dx = (SHORT)(x - g_legacy_x), dy = (SHORT)(y - g_legacy_y);
            g_legacy_x = x; g_legacy_y = y;
            if (dx || dy) {
                if (g_armed) {
                    hm_motion(&g_legacy_mouse, dx, dy, now);
                    DIAG_INC(relative_motion);
                } else DIAG_INC(disarmed);
            }
        }
    }
    if (!g_armed && hm_button_state_released(&g_button_state)) g_armed = 1;
}

static HWND read_game_window(DWORD *thread_out) {
    DWORD base;
    if (!supported_exe(&base)) return NULL;
    HWND hwnd = *(HWND *)(base + (GAME_HWND_VA - EXE_PREFERRED_BASE));
    DWORD pid = 0;
    DWORD tid = hwnd ? GetWindowThreadProcessId(hwnd, &pid) : 0;
    if (!hwnd || !IsWindow(hwnd) || !tid || pid != GetCurrentProcessId()) return NULL;
    if (thread_out) *thread_out = tid;
    return hwnd;
}

static void monitor_game_window(void) {
#ifdef HOGSMOUSE_TEST
    return;
#else
    if (!g_installed || g_permanent_failure || !g_game_thread || GetCurrentThreadId() != g_game_thread) return;
    DWORD candidate_thread = 0; HWND candidate = read_game_window(&candidate_thread);
    if (candidate == g_hwnd) return;
    clear_and_disarm("Game window changed; mouse state cleared."); unregister_mouse(); g_focused = 0;
    if (!candidate) { g_hwnd = NULL; DIAG_INC(rebinds); return; }
    if (candidate_thread != g_game_thread || candidate_thread != GetCurrentThreadId()) {
        DIAG_INC(rebind_fail); g_permanent_failure = 1;
        log_line("Replacement game window belongs to another thread; mouse input disabled safely."); return;
    }
    g_hwnd = candidate;
    if (!register_mouse(candidate)) log_line("Raw Input unavailable after window change; legacy foreground mouse fallback remains enabled.");
    g_focused = is_game_foreground(); g_armed = 0; DIAG_INC(rebinds);
    log_line("Game message target updated; release both buttons before aiming or firing.");
#endif
}

static void apply_raw_button_flags(USHORT flags, int allow_down) {
    if ((flags & RI_MOUSE_LEFT_BUTTON_DOWN) && allow_down) { record_button_event(HM_MOUSE_LEFT, 1); DIAG_INC(raw_l_down); }
    if (flags & RI_MOUSE_LEFT_BUTTON_UP) { record_button_event(HM_MOUSE_LEFT, 0); DIAG_INC(raw_l_up); }
    if ((flags & RI_MOUSE_RIGHT_BUTTON_DOWN) && allow_down) { record_button_event(HM_MOUSE_RIGHT, 1); DIAG_INC(raw_r_down); }
    if (flags & RI_MOUSE_RIGHT_BUTTON_UP) { record_button_event(HM_MOUSE_RIGHT, 0); DIAG_INC(raw_r_up); }
    if ((flags & (RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_UP)) && !allow_down) reset_aim_motion();
    if (flags) DIAG_INC(button_events);
    if (!g_armed && g_enabled && g_focused && is_game_foreground() && !game_ui_active() && hm_button_state_released(&g_button_state)) g_armed = 1;
}

static void process_rawinput(HWND hwnd, HRAWINPUT handle) {
    DIAG_INC(wm_input);
    if (!g_installed || hwnd != g_hwnd || GetCurrentThreadId() != g_game_thread) return;
    int active = game_input_gate(hwnd);
    RAWINPUT raw; UINT size = sizeof(raw);
    UINT read = GetRawInputData(handle, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));
    if (read == (UINT)-1 || read < offsetof(RAWINPUT, data) + sizeof(RAWMOUSE)) DIAG_INC(raw_read_fail);
    else if (raw.header.dwType != RIM_TYPEMOUSE) DIAG_INC(nonmouse);
    else {
        DWORD now = now_ms();
        apply_raw_button_flags(raw.data.mouse.usButtonFlags, active);
        if (!active) return;
        g_raw_seen = 1; g_raw_event_ms = now;
        hm_clear(&g_legacy_mouse); g_legacy_active = 0; g_legacy_baseline = 0;
        if (raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) DIAG_INC(absolute);
        else if (!g_armed) DIAG_INC(disarmed);
        else if (!raw.data.mouse.lLastX && !raw.data.mouse.lLastY) DIAG_INC(zero_motion);
        else { hm_motion(&g_mouse, raw.data.mouse.lLastX, raw.data.mouse.lLastY, now); DIAG_INC(relative_motion); }
    }
}

static void poll_focus_state(void) {
    int foreground = g_hwnd && is_game_foreground();
    if (g_focused && !foreground) { g_focused = 0; g_f6_down = 0; clear_and_disarm("Focus lost: held inputs and pending movement cleared."); }
    else if (!g_focused && foreground) { g_focused = 1; g_f6_down = 0; clear_and_disarm("Focus returned: release both buttons before rearming."); g_armed = 0; }
}

static void process_removed_message(const MSG *m);
static int inspect_queue_result(int api, int result, unsigned remove, const MSG *m) {
    if (!m || !hm_should_inspect_removed_message(api, result, remove, (uintptr_t)m->hwnd,
            (uintptr_t)g_hwnd, GetCurrentThreadId(), g_game_thread)) return 0;
#ifdef HOGSMOUSE_TEST
    MSG before = *m;
#endif
    process_removed_message(m);
#ifdef HOGSMOUSE_TEST
    if (memcmp(m, &before, sizeof(before))) InterlockedIncrement(&g_test_msg_mutations);
#endif
    return 1;
}
#ifdef HOGSMOUSE_TEST
int hm_test_inspect_queue_result(int api, int result, unsigned remove, const MSG *m) {
    return inspect_queue_result(api, result, remove, m);
}
#endif

static void process_removed_message(const MSG *m) {
    if (!m || !g_installed || !g_hwnd || m->hwnd != g_hwnd || GetCurrentThreadId() != g_game_thread) return;
    poll_focus_state(); poll_control_state();
    if (m->message == WM_INPUT) process_rawinput(m->hwnd, (HRAWINPUT)m->lParam);
    else if (m->message == WM_MOUSEMOVE || m->message == WM_LBUTTONDOWN || m->message == WM_LBUTTONUP ||
             m->message == WM_RBUTTONDOWN || m->message == WM_RBUTTONUP)
        process_legacy_mouse(m->hwnd, m->message, m->wParam, m->lParam);
    else if (m->message == WM_KEYUP && m->wParam == VK_F6) g_f6_down = 0;
    else if (m->message == WM_KEYDOWN && m->wParam == VK_F6 && !(m->lParam & (1L << 30)) &&
             !g_f6_down && !g_permanent_failure && g_focused && is_game_foreground() &&
             g_last_game_input_ms && (uint32_t)(now_ms() - g_last_game_input_ms) <= 100 && !game_ui_active()) {
        g_f6_down = 1; g_enabled = !g_enabled;
        clear_and_disarm(g_enabled ? "Mouse control enabled; release buttons to rearm." : "Mouse control disabled.");
    } else if (m->message == WM_KEYDOWN && m->wParam == VK_ESCAPE) clear_and_disarm("Escape: mouse inputs cleared; release buttons to rearm.");
    else if (m->message == WM_KILLFOCUS || (m->message == WM_ACTIVATEAPP && !m->wParam)) {
        g_focused = 0; g_f6_down = 0; clear_and_disarm("Focus lost: held inputs and pending movement cleared.");
    } else if (m->message == WM_SETFOCUS || (m->message == WM_ACTIVATEAPP && m->wParam)) {
        g_focused = is_game_foreground(); g_f6_down = 0; clear_and_disarm(NULL); g_armed = 0;
    } else if (m->message == WM_CANCELMODE || m->message == WM_CAPTURECHANGED) clear_and_disarm("Window capture/state changed: mouse inputs cleared.");
    else if (m->message == WM_DESTROY) {
        clear_and_disarm("Game window destroyed: mouse inputs cleared."); unregister_mouse(); g_focused = 0; g_hwnd = NULL;
    }
}

static int merge_mouse(BYTE *buf, BYTE *user) {
    if (g_permanent_failure || !g_enabled || !g_installed || !g_armed || !g_focused || !is_game_foreground()) return 0;
    if (game_ui_active()) { clear_and_disarm(NULL); return 0; }
    DWORD now = now_ms();
    int source = hm_poll_choose_source(g_raw_registered && g_raw_registration_matches,
        g_raw_seen, g_raw_event_ms, g_legacy_active, g_legacy_event_ms,
        g_poll_mouse.sample_valid, now);
    /* A source change ends the previous gesture: never carry sub-step
       movement from Raw Input into legacy/poll, or vice versa. */
    if (source != g_aim_source) {
        g_mouse.remainder_dx = g_mouse.remainder_dy = 0;
        g_legacy_mouse.remainder_dx = g_legacy_mouse.remainder_dy = 0;
        g_poll_mouse.mouse.remainder_dx = g_poll_mouse.mouse.remainder_dy = 0;
        g_aim_source = source;
    }
    if (source == HM_SOURCE_POLL) DIAG_INC(poll_chosen);
    HMState *state = source == HM_SOURCE_RAW ? &g_mouse :
                     source == HM_SOURCE_LEGACY ? &g_legacy_mouse :
                     source == HM_SOURCE_POLL ? &g_poll_mouse.mouse : NULL;
    HMState no_motion = {0};
    uint16_t mask = hm_button_state_take_fire(&g_button_state);
    HMState *aim_state = state ? state : &no_motion;
    uint16_t aim = hm_read_aim(aim_state, now, g_sensitivity, g_invert_y,
        g_require_rmb, (g_button_state.held & HM_MOUSE_RIGHT) != 0);
    mask |= aim;
    if (aim & HM_UP) DIAG_INC(aim_up);
    if (aim & HM_DOWN) DIAG_INC(aim_down);
    if (aim & HM_LEFT) DIAG_INC(aim_left);
    if (aim & HM_RIGHT) DIAG_INC(aim_right);
    if (aim & HM_L1) DIAG_INC(aim_l1);
    else if (g_require_rmb) DIAG_INC(aim_rmb_gated);
    if (state != &g_mouse) g_mouse.dx = g_mouse.dy = 0;
    if (state != &g_legacy_mouse) g_legacy_mouse.dx = g_legacy_mouse.dy = 0;
    if (state != &g_poll_mouse.mouse) g_poll_mouse.mouse.dx = g_poll_mouse.mouse.dy = 0;
    if (!mask) return 0;
    const WORD *bind = (const WORD *)(user + 0x108);
    /* Same contract as HogsFix: returning success when synthetic keys were added. */
    int applied = hm_apply_bindings(buf, bind, mask);
    if (applied) g_observer_mouse_mask = mask;
    if (applied) DIAG_INC(merge_added);
    else DIAG_INC(merge_invalid_bind);
    return applied;
}

/* Call the saved HogsFix naked stub with its real ABI: one stack parameter and
   the current player pointer in EDI. The C input processor must not call the
   stub as if it were a two-argument C function. */
static BYTE __cdecl call_prior_stub(BYTE *buf, BYTE *user);
static __declspec(naked) BYTE __cdecl call_prior_stub(BYTE *buf, BYTE *user) {
    __asm {
        push edi
        mov edi, dword ptr [esp + 12]
        push dword ptr [esp + 8]
        call dword ptr [g_prior_kb]
        add esp, 4
        pop edi
        ret
    }
}

static BYTE __cdecl chained_kb_state(BYTE *buf, BYTE *user) {
    DIAG_INC(callback);
    BYTE ok = g_prior_kb ? call_prior_stub(buf, user) : 0;
    HogsObserverInputKeys observer_keys;
    memset(&observer_keys, 0, sizeof(observer_keys));
    if (g_installed && g_armed && g_focused && GetCurrentThreadId() == g_game_thread &&
        is_game_foreground() && !game_ui_active())
        hogs_observer_capture_keys((uintptr_t)buf, (uintptr_t)user, ok, &observer_keys);
    if (!ok) DIAG_INC(callback_ok0);
    if (!g_installed) return ok;
    if (GetCurrentThreadId() != g_game_thread) { DIAG_INC(callback_thread); return ok; } /* all state stays on the HWND thread */
    DWORD now = now_ms();
    if (game_ui_active()) {
        DIAG_INC(callback_ui);
        clear_and_disarm(NULL);
        return ok;
    }
    if (g_last_game_input_ms && (uint32_t)(now - g_last_game_input_ms) > 100) {
        hm_button_disarm(&g_button_state);
        reset_aim_motion(); g_raw_seen = g_legacy_active = g_legacy_baseline = 0;
        g_armed = physical_buttons_up();
    }
    g_last_game_input_ms = now;
    if (!ok && buf) memset(buf, 0, 256);
    if (buf && user && g_installed && g_armed && g_focused && is_game_foreground()) sample_poll_mouse(now);
    g_observer_mouse_mask = 0;
    int added = (buf && user) ? merge_mouse(buf, user) : 0;
    observer_tick(user, ok, &observer_keys);
    return added ? 1 : ok;
}

static __declspec(naked) void mouse_kb_stub(void) {
    __asm {
        push edi
        push dword ptr [esp + 8]
        call chained_kb_state
        add esp, 8
        ret
    }
}

static int keyboard_callsite_owned(void) {
    DWORD base;
    if (!supported_exe(&base)) return 0;
    BYTE *site = (BYTE *)(base + (KB_CALL_VA - EXE_PREFERRED_BASE));
    if (site[0] != 0xe8) return 0;
    return (intptr_t)(site + 5) + *(int32_t *)(site + 1) == (intptr_t)mouse_kb_stub;
}

static int install_kb_chain(void) {
    DWORD base;
    if (!supported_exe(&base)) { log_line("Unsupported game executable build; no hooks installed."); g_permanent_failure = 1; return 0; }
    HMODULE core = GetModuleHandleA("HogsFix.asi");
    if (!core) return 0;
    DWORD site = base + (KB_CALL_VA - EXE_PREFERRED_BASE);
    BYTE *call = (BYTE *)site;
    if (call[0] != 0xe8) { log_line("Known keyboard callsite signature missing; no hooks installed."); g_permanent_failure = 1; return 0; }
    intptr_t prior = (intptr_t)(site + 5) + *(int32_t *)(call + 1);
    if (!module_contains(core, (void *)prior)) {
        log_line("HogsFix gamepad hook is absent (or disabled); mouse add-on stays off.");
        g_permanent_failure = 1;
        return 0;
    }
    /* HogsFix's known stub starts push edi; push [esp+8]; call rel32. */
    static const BYTE sig[5] = { 0x57, 0xff, 0x74, 0x24, 0x08 };
    static const BYTE tail[4] = { 0x83, 0xc4, 0x08, 0xc3 };
    if (!module_contains_range(core, (void *)prior, 14) || memcmp((void *)prior, sig, sizeof(sig)) ||
        *((BYTE *)prior + 5) != 0xe8 || memcmp((BYTE *)prior + 10, tail, sizeof(tail))) {
        log_line("HogsFix target is in its image but has an unknown ABI signature; no hooks installed.");
        g_permanent_failure = 1;
        return 0;
    }
    intptr_t helper = prior + 10 + *(int32_t *)(prior + 6);
    if (!module_contains(core, (void *)helper)) {
        log_line("HogsFix stub helper is outside the core image; no hooks installed.");
        g_permanent_failure = 1;
        return 0;
    }
    g_prior_kb = (KB_TARGET)prior;
    return 1;
}

static void load_config(void) {
    char ini[MAX_PATH];
    DWORD n = GetModuleFileNameA(g_self, ini, MAX_PATH);
    while (n && ini[n - 1] != '\\') --n;
    if (n < MAX_PATH - 14) lstrcpyA(ini + n, "HogsMouse.ini"); else return;
    g_enabled = GetPrivateProfileIntA("Mouse", "Enable", 1, ini) != 0;
    g_sensitivity = GetPrivateProfileIntA("Mouse", "PixelsPerStep", 4, ini);
    if (g_sensitivity < 1) g_sensitivity = 1;
    if (g_sensitivity > 64) g_sensitivity = 64;
    g_invert_y = GetPrivateProfileIntA("Mouse", "InvertY", 0, ini) != 0;
    int configured_rmb = GetPrivateProfileIntA("Mouse", "RequireRightButton", 1, ini) != 0;
    g_require_rmb = hm_normalize_require_rmb(configured_rmb);
    if (!configured_rmb)
        log_line("RequireRightButton=0 is an old free-aim setting and is ignored; hold the right mouse button to aim.");
    if (GetPrivateProfileIntA("Mouse", "CaptureCursor", 0, ini))
        log_line("CaptureCursor is reserved and unsupported; cursor remains free.");
    g_cfg_loaded = 1;
}

#ifdef HOGSMOUSE_TEST
/* Test-only ABI seam; absent from release builds. */
void hm_test_set_prior(KB_TARGET prior) { g_prior_kb = prior; }
__declspec(naked) void hm_test_call_stub(void) { __asm { jmp mouse_kb_stub } }
void hm_test_reset_state(void) {
    memset(&g_mouse, 0, sizeof(g_mouse)); memset(&g_legacy_mouse, 0, sizeof(g_legacy_mouse));
    hm_button_state_clear(&g_button_state);
    hm_poll_reset(&g_poll_mouse);
    g_hwnd = (HWND)1; g_game_thread = GetCurrentThreadId();
    g_enabled = g_installed = g_armed = g_focused = 1;
    g_initializing = g_permanent_failure = g_f6_down = 0;
    g_test_now = 1000; g_test_buttons = 0; g_test_foreground = 1; g_test_native_focus = 1; g_test_ui = 0; g_test_old_calls = 0;
    g_test_cursor_x = 100; g_test_cursor_y = 100; g_test_cursor_valid = 1;
    g_last_game_input_ms = 1000; g_sensitivity = 4; g_invert_y = 0; g_require_rmb = 1; g_aim_source = HM_SOURCE_NONE;
    g_raw_registered = g_raw_seen = g_legacy_active = g_legacy_baseline = 0;
    g_raw_registration_matches = 0; g_raw_registration_verified = 0; g_raw_registration_check_at = 0;
    g_test_raw_registration_match = 1; d_raw_read_fail = 0; g_test_msg_mutations = 0;
    g_raw_event_ms = g_legacy_event_ms = 0; g_legacy_x = g_legacy_y = 0;
}
void hm_test_set_now(DWORD now) { g_test_now = now; }
void hm_test_set_buttons(BYTE buttons) { g_test_buttons = buttons; }
void hm_test_set_raw_registration_match(int matches) { g_test_raw_registration_match = matches != 0; }
void hm_test_set_require_rmb(int enabled) { g_require_rmb = hm_normalize_require_rmb(enabled); }
void hm_test_set_cursor(LONG x, LONG y, int valid) { g_test_cursor_x=x; g_test_cursor_y=y; g_test_cursor_valid=valid; }
BYTE hm_test_poll_held(void) { return g_poll_mouse.mouse.held; }
int hm_test_poll_dx(void) { return g_poll_mouse.mouse.dx; }
int hm_test_poll_baseline(void) { return g_poll_mouse.baseline_valid; }
int hm_test_poll_sample_valid(void) { return g_poll_mouse.sample_valid; }
LONG hm_test_poll_samples(void) { return d_poll_sample; }
void hm_test_set_foreground(int active) { g_test_foreground = active; }
void hm_test_set_native_focus(int active) { g_test_native_focus = active; }
void hm_test_set_ui(int active) { g_test_ui = active; }
void hm_test_set_permanent_failure(int active) { g_permanent_failure = active; }
void hm_test_seed_mouse(BYTE held, BYTE pressed, int dx, int dy, DWORD last_motion) {
    /* This seam represents state produced by a valid Raw Input event. Keep its
       source/registration metadata consistent with the production WM_INPUT path. */
    g_raw_seen = g_raw_registered = g_raw_registration_matches = 1;
    g_raw_event_ms = now_ms();
    g_mouse.held = held; g_mouse.pressed = pressed; g_mouse.dx = dx; g_mouse.dy = dy; g_mouse.last_motion_ms = last_motion;
}
void hm_test_seed_raw(int seen, DWORD last_event) {
    g_raw_seen = seen; g_raw_event_ms = last_event;
    g_raw_registered = seen; g_raw_registration_matches = seen;
}
int hm_test_selected_source(void) { return hm_poll_choose_source(g_raw_registered && g_raw_registration_matches, g_raw_seen, g_raw_event_ms, g_legacy_active, g_legacy_event_ms, g_poll_mouse.sample_valid, now_ms()); }

void hm_test_legacy_event(UINT msg, WPARAM wp, LPARAM lp) { process_legacy_mouse(g_hwnd, msg, wp, lp); }
int hm_test_legacy_dx(void) { return g_legacy_mouse.dx; }
int hm_test_legacy_held(void) { return g_legacy_mouse.held; }
int hm_test_legacy_pressed(void) { return g_legacy_mouse.pressed; }
int hm_test_legacy_active(void) { return g_legacy_active; }
LONG hm_test_message_mutations(void) { return g_test_msg_mutations; }
/* Model real WM_INPUT button events: the production handler records source
   ownership/time before applying down/up transitions. */
void hm_test_raw_button_event(USHORT flags) {
    g_raw_seen = g_raw_registered = g_raw_registration_matches = g_raw_registration_verified = 1;
    g_raw_event_ms = now_ms();
    apply_raw_button_flags(flags, 1);
}
void hm_test_raw_motion(LONG dx, LONG dy) {
    g_raw_seen = g_raw_registered = g_raw_registration_matches = g_raw_registration_verified = 1;
    g_raw_event_ms = now_ms();
    if (g_armed && (dx || dy)) hm_motion(&g_mouse, dx, dy, now_ms());
}
BYTE hm_test_event_buttons_held(void) { return g_button_state.held; }
BYTE hm_test_event_buttons_known(void) { return g_button_state.known; }
BYTE hm_test_event_buttons_pressed(void) { return g_button_state.pressed; }
BYTE hm_test_event_buttons_wait_release(void) { return g_button_state.require_event_release; }
int hm_test_rearm_ready(void) { return hm_button_state_released(&g_button_state); }
void hm_test_queue_message(const MSG *m) { process_removed_message(m); }
LONG hm_test_raw_read_fail_count(void) { return d_raw_read_fail; }
void hm_test_set_last_game_input(DWORD now) { g_last_game_input_ms = now; }
void hm_test_set_window(HWND hwnd) { g_hwnd = hwnd; g_game_thread = GetCurrentThreadId(); g_installed = 1; g_enabled = 1; g_focused = 1; g_armed = 1; g_last_game_input_ms = now_ms(); }
int hm_test_install_queue_hooks(void) {
    HMODULE exe = GetModuleHandleA(NULL);
    void **slot = find_iat(exe, "user32.dll", "PeekMessageA");
    if (slot) { g_real_peek = (BOOL (WINAPI *)(LPMSG, HWND, UINT, UINT, UINT))*slot; patch_iat(slot, (void *)mouse_peek); }
    slot = find_iat(exe, "user32.dll", "GetMessageA");
    if (slot) { g_real_get = (int (WINAPI *)(LPMSG, HWND, UINT, UINT))*slot; patch_iat(slot, (void *)mouse_get); }
    void **peek_slot = find_iat(exe, "user32.dll", "PeekMessageA");
    void **get_slot = find_iat(exe, "user32.dll", "GetMessageA");
    return g_real_peek && g_real_get && peek_slot && get_slot &&
           *peek_slot == (void *)mouse_peek && *get_slot == (void *)mouse_get;
}
void hm_test_poll_state(void) { poll_control_state(); }
BYTE hm_test_run_callback(BYTE *buf, BYTE *user) { return chained_kb_state(buf, user); }
int hm_test_merge(BYTE *buf, BYTE *user) { return merge_mouse(buf, user); }
int hm_test_is_armed(void) { return g_armed; }
int hm_test_is_enabled(void) { return g_enabled; }
int hm_test_old_calls(void) { return 0; }
BYTE hm_test_mouse_held(void) { return g_mouse.held; }
BYTE hm_test_mouse_pressed(void) { return g_mouse.pressed; }
int hm_test_mouse_dx(void) { return g_mouse.dx; }
int hm_test_legacy_motion_dx(void) { return g_legacy_mouse.dx; }
#endif

static void try_initialize(void) {
#ifdef HOGSMOUSE_TEST
    return;
#else
    if (g_installed || g_initializing || g_permanent_failure) return;
    g_initializing = 1;
    DWORD pid = 0;
    DWORD base;
    if (!supported_exe(&base)) { log_line("Unsupported game executable; mouse add-on disabled."); g_permanent_failure = 1; goto done; }
    HWND hwnd = (HWND)*(DWORD *)(base + (GAME_HWND_VA - EXE_PREFERRED_BASE));
    if (!hwnd || !IsWindow(hwnd) || GetWindowThreadProcessId(hwnd, &pid) != GetCurrentThreadId() || pid != GetCurrentProcessId() ||
        GetForegroundWindow() != hwnd) goto done;
    if (!g_cfg_loaded) load_config();
    if (!g_enabled) goto done;
    if (!install_kb_chain()) goto done;
    g_hwnd = hwnd; g_game_thread = GetCurrentThreadId(); g_focused = 1;
    if (!register_mouse(hwnd))
        log_line("Raw Input unavailable; legacy foreground mouse fallback remains enabled.");
    BYTE *site = (BYTE *)((DWORD)GetModuleHandleA(NULL) + (KB_CALL_VA - EXE_PREFERRED_BASE));
    if (site[0] != 0xe8 || (intptr_t)(site + 5) + *(int32_t *)(site + 1) != (intptr_t)g_prior_kb) {
        log_line("Callsite changed before install; rolling back mouse registration.");
        unregister_mouse(); g_permanent_failure = 1; goto done;
    }
    DWORD disp = (DWORD)((BYTE *)mouse_kb_stub - (site + 5));
    DWORD old;
    if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old)) {
        log_line("Could not patch verified input callsite; rolling back mouse registration.");
        unregister_mouse(); g_permanent_failure = 1; goto done;
    }
    site[1] = (BYTE)disp; site[2] = (BYTE)(disp >> 8); site[3] = (BYTE)(disp >> 16); site[4] = (BYTE)(disp >> 24);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    VirtualProtect(site, 5, old, &old);
    g_installed = 1; g_armed = physical_buttons_up();
    log_line("Mouse control installed; hold right mouse to aim, left mouse to fire; F6 toggles.");
done:
    g_initializing = 0;
#endif
}

static void poll_control_state(void) {
    if (!g_game_thread || GetCurrentThreadId() != g_game_thread) return;
    poll_focus_state();
    refresh_raw_registration();
    if (g_installed && g_hwnd && game_ui_active()) {
        if (g_armed || g_button_state.held || g_button_state.pressed || g_mouse.dx || g_mouse.dy || g_legacy_mouse.dx || g_legacy_mouse.dy || g_poll_mouse.mouse.dx || g_poll_mouse.mouse.dy)
            clear_and_disarm(NULL);
        return;
    }
    sync_button_poll();
    if (g_last_game_input_ms && (uint32_t)(now_ms() - g_last_game_input_ms) > 100) {
        hm_button_disarm(&g_button_state);
        reset_aim_motion(); g_raw_seen = g_legacy_active = g_legacy_baseline = 0;
        g_last_game_input_ms = 0;
        g_armed = 0;
    }
    if (g_hwnd && is_game_foreground() && !g_armed && hm_button_state_released(&g_button_state)) {
        g_armed = 1;
    }
}

static BOOL WINAPI mouse_peek(LPMSG m, HWND h, UINT lo, UINT hi, UINT remove) {
    BOOL result = g_real_peek ? g_real_peek(m, h, lo, hi, remove) : FALSE;
    try_initialize(); monitor_game_window(); poll_control_state();
    inspect_queue_result(HM_QUEUE_PEEK, result, remove, m);
    DIAG_INC(peek); report_diagnostics(); return result;
}

static int WINAPI mouse_get(LPMSG m, HWND h, UINT lo, UINT hi) {
    int result = g_real_get ? g_real_get(m, h, lo, hi) : -1;
    try_initialize(); monitor_game_window(); poll_control_state();
    inspect_queue_result(HM_QUEUE_GET, result, 0, m);
    DIAG_INC(get); report_diagnostics(); return result;
}

static void patch_iat(void **slot, void *fn) {
    DWORD old;
    if (!slot || !VirtualProtect(slot, 4, PAGE_READWRITE, &old)) return;
    *(DWORD *)slot = (DWORD)fn;
    VirtualProtect(slot, 4, old, &old);
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = h;
        DisableThreadLibraryCalls(h);
        void **slot = find_iat(GetModuleHandleA(NULL), "user32.dll", "PeekMessageA");
        if (slot) {
            g_real_peek = (BOOL (WINAPI *)(LPMSG, HWND, UINT, UINT, UINT))*slot;
            patch_iat(slot, (void *)mouse_peek);
        }
        slot = find_iat(GetModuleHandleA(NULL), "user32.dll", "GetMessageA");
        if (slot) { g_real_get = (int (WINAPI *)(LPMSG, HWND, UINT, UINT))*slot; patch_iat(slot, (void *)mouse_get); }
    }
    return TRUE;
}
