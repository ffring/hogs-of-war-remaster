#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "native_runtime.h"
#include "runtime_core.h"
#include "../native-menu-v1/menu_bridge.h"
#include "../native-ini-v1/native_ini.h"
#include "../native-render-v1/native_render.h"
#include "../loading-prompt-v1/native_wait_policy.h"

/* Implemented by the native renderer; draws FE17 and FE21..24 in the stock batch. */
int hmr_draw_options(int selected);

enum { HNR_ERROR_NONE = 0, HNR_ERROR_READ = 1, HNR_ERROR_SAVE = 2 };

typedef struct HnrRuntime {
    char ini_path[MAX_PATH];
    HnrSetWaitFn set_wait_active;
    HnrClearInputFn clear_input;
    HogsMouseSettings settings;
    HMMenuState menu;
    HogsWaitInputPolicy wait_policy;
    uint8_t *options_node;
    uint32_t *action_rows;
    uint32_t *target_rows;
    uint32_t original_count;
    uint32_t original_action_pointer;
    uint32_t original_target_pointer;
    int initialized;
    int row_installed;
    int can_save;
    int error;
    int suppressed;
    int permanent_row_failure;
} HnrRuntime;

static HnrRuntime g_runtime;
static uint32_t g_action_rows[HNR_OPTIONS_EXTENDED_COUNT];
static uint32_t g_target_rows[HNR_OPTIONS_EXTENDED_COUNT];
/* Text lookup returns the encoded FE byte stream (ASCII glyph minus 0x1f). */
static const char g_mouse_label[] = {0x2e, 0x34, 0x2a, 0x38, 0x34, 0};

static int hnr_region_ok(const void *address, size_t size, int write_access) {
    MEMORY_BASIC_INFORMATION info;
    uintptr_t start = (uintptr_t)address;
    uintptr_t end;
    DWORD p;
    if (!address || !size || size > UINTPTR_MAX - start) return 0;
    end = start + size;
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info) ||
        info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return 0;
    if (end > (uintptr_t)info.BaseAddress + info.RegionSize) return 0;
    p = info.Protect & 0xffu;
    if (write_access)
        return p == PAGE_READWRITE || p == PAGE_WRITECOPY ||
               p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
    return p == PAGE_READONLY || p == PAGE_READWRITE || p == PAGE_WRITECOPY ||
           p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE ||
           p == PAGE_EXECUTE_WRITECOPY;
}

static int hnr_read(const void *address, void *out, size_t size) {
    if (!out || !hnr_region_ok(address, size, 0)) return 0;
    memcpy(out, address, size);
    return 1;
}

static int hnr_write(void *address, const void *value, size_t size) {
    if (!value || !hnr_region_ok(address, size, 1)) return 0;
    memcpy(address, value, size);
    return 1;
}

static int hnr_snapshot(HnrOptionsSnapshot *out, uint8_t **node_out,
                        uint32_t *current_out) {
    uint32_t root_address = 0, current_address = 0;
    uint8_t *root, *node;
    if (!out || !node_out || !current_out ||
        !hnr_read((const void *)(uintptr_t)HNR_OPTIONS_ROOT_GLOBAL,
                  &root_address, sizeof(root_address)) ||
        !hnr_read((const void *)(uintptr_t)HNR_CURRENT_NODE_GLOBAL,
                  &current_address, sizeof(current_address))) return 0;
    root = (uint8_t *)(uintptr_t)root_address;
    if (!root_address || root_address < 0x10000u ||
        !hnr_region_ok(root, 52u * HNR_NODE_STRIDE, 0)) return 0;
    node = root + HNR_OPTIONS_NODE_INDEX * HNR_NODE_STRIDE;
    if (!hnr_region_ok(node, HNR_NODE_STRIDE, 0) ||
        !hnr_read(node + 0x00, &out->count, sizeof(uint32_t)) ||
        !hnr_read(node + 0x04, &out->title_id, sizeof(uint32_t)) ||
        !hnr_read(node + 0x08, &out->first_text_id, sizeof(uint32_t)) ||
        !hnr_read(node + 0x0C, &out->selection, sizeof(uint32_t)) ||
        !hnr_read(node + 0x18, &out->action_table, sizeof(uint32_t)) ||
        !hnr_read(node + 0x1C, &out->target_table, sizeof(uint32_t))) return 0;
    *node_out = node;
    *current_out = current_address;
    return 1;
}

static int hnr_frontend_active(void) {
    uint32_t frontend = 0;
    return hnr_read((const void *)(uintptr_t)HNR_FRONTEND_GLOBAL,
                    &frontend, sizeof(frontend)) && frontend != 0;
}

static int hnr_read_original_rows(const HnrOptionsSnapshot *snapshot,
                                  uint32_t action[3], uint32_t target[3]) {
    if (!snapshot || snapshot->action_table != HNR_ORIGINAL_ACTION_TABLE ||
        snapshot->target_table != HNR_ORIGINAL_TARGET_TABLE ||
        !hnr_read((const void *)(uintptr_t)snapshot->action_table, action,
                  3u * sizeof(uint32_t)) ||
        !hnr_read((const void *)(uintptr_t)snapshot->target_table, target,
                  3u * sizeof(uint32_t))) return 0;
    return 1;
}

static int hnr_install_options_row(void) {
    HnrOptionsSnapshot s;
    uint8_t *node = NULL;
    uint32_t current = 0, action[3], target[3];
    uint32_t extended_action[4], extended_target[4];
    uint32_t action_address, target_address, four = HNR_OPTIONS_EXTENDED_COUNT;
    if (g_runtime.permanent_row_failure) return 0;
    if (!hnr_frontend_active() || !hnr_snapshot(&s, &node, &current) ||
        current != (uint32_t)(uintptr_t)node ||
        s.title_id != HNR_OPTIONS_TITLE_ID || s.first_text_id != HNR_OPTIONS_FIRST_TEXT_ID)
        return 0;
    action_address = (uint32_t)(uintptr_t)g_action_rows;
    target_address = (uint32_t)(uintptr_t)g_target_rows;
    if (hnr_options_extended_matches(&s, action_address, target_address)) {
        if (!hnr_read(g_action_rows, extended_action, sizeof(extended_action)) ||
            !hnr_read(g_target_rows, extended_target, sizeof(extended_target)) ||
            !hnr_extended_options_tables(extended_action, extended_target)) goto fail_closed;
        g_runtime.options_node = node;
        g_runtime.action_rows = g_action_rows;
        g_runtime.target_rows = g_target_rows;
        g_runtime.original_count = HNR_OPTIONS_ORIGINAL_COUNT;
        g_runtime.original_action_pointer = HNR_ORIGINAL_ACTION_TABLE;
        g_runtime.original_target_pointer = HNR_ORIGINAL_TARGET_TABLE;
        g_runtime.row_installed = 1;
        return 1;
    }
    if (!hnr_read_original_rows(&s, action, target) ||
        !hnr_options_base_matches(&s, action, target) ||
        !hnr_build_options_tables(action, target, g_action_rows, g_target_rows) ||
        !hnr_region_ok(node, HNR_NODE_STRIDE, 1)) goto fail_closed;

    g_runtime.options_node = node;
    g_runtime.original_count = s.count;
    g_runtime.original_action_pointer = s.action_table;
    g_runtime.original_target_pointer = s.target_table;
    g_runtime.action_rows = g_action_rows;
    g_runtime.target_rows = g_target_rows;
    /* Publish pointers before the larger count, all on the UI thread. */
    if (!hnr_write(node + 0x18, &action_address, sizeof(action_address)) ||
        !hnr_write(node + 0x1C, &target_address, sizeof(target_address)) ||
        !hnr_write(node + 0x00, &four, sizeof(four))) {
        (void)hnr_write(node + 0x00, &s.count, sizeof(s.count));
        (void)hnr_write(node + 0x18, &s.action_table, sizeof(s.action_table));
        (void)hnr_write(node + 0x1C, &s.target_table, sizeof(s.target_table));
        goto fail_closed;
    }
    g_runtime.row_installed = 1;
    return 1;

fail_closed:
    g_runtime.permanent_row_failure = 1;
    return 0;
}

static int hnr_current_options(HnrOptionsSnapshot *snapshot) {
    HnrOptionsSnapshot local;
    uint8_t *node = NULL;
    uint32_t current = 0;
    if (!snapshot) snapshot = &local;
    if (!g_runtime.row_installed || !hnr_frontend_active() ||
        !hnr_snapshot(snapshot, &node, &current) ||
        node != g_runtime.options_node ||
        !hnr_extended_options_tables(g_action_rows, g_target_rows) ||
        !hnr_is_active_options_context(1, snapshot, current, (uint32_t)(uintptr_t)node,
                                       (uint32_t)(uintptr_t)g_runtime.action_rows,
                                       (uint32_t)(uintptr_t)g_runtime.target_rows)) return 0;
    return 1;
}

static void hnr_sync_host_state(void) {
    int suppressed = g_runtime.menu.active || g_runtime.wait_policy.active;
    if (suppressed == g_runtime.suppressed) return;
    g_runtime.suppressed = suppressed;
    if (g_runtime.set_wait_active) g_runtime.set_wait_active(suppressed);
    if (g_runtime.clear_input) g_runtime.clear_input();
}

static void hnr_close_menu(void) {
    g_runtime.menu.active = 0;
    g_runtime.menu.status = HM_MENU_STATUS_CANCELLED;
    g_runtime.can_save = 0;
    hnr_sync_host_state();
}

static int hnr_load_menu_settings(void) {
    HogsMouseSettings loaded;
    HogsIniResult result = hogs_ini_load(g_runtime.ini_path, &loaded);
    if (result == HOGS_INI_OK) {
        g_runtime.settings = loaded;
        g_runtime.can_save = 1;
        g_runtime.error = HNR_ERROR_NONE;
    } else {
        g_runtime.settings = hogs_ini_defaults();
        g_runtime.can_save = 0;
        g_runtime.error = HNR_ERROR_READ;
    }
    return result == HOGS_INI_OK;
}

static int hnr_open_menu(void) {
    HnrOptionsSnapshot s;
    if (!hmr_ready() || !hnr_current_options(&s) || s.selection != 3u) return 0;
    (void)hnr_load_menu_settings();
    if (!hm_menu_open(&g_runtime.menu, 1,
                      g_runtime.settings.native_units_per_pixel,
                      g_runtime.settings.aim_sensitivity_percent,
                      g_runtime.settings.invert_x, g_runtime.settings.invert_y)) return 0;
    hnr_sync_host_state();
    return 1;
}

static void hnr_finish_save(void) {
    HogsMouseSettings next;
    HogsIniResult result;
    next.native_units_per_pixel = g_runtime.menu.turn_units;
    next.aim_sensitivity_percent = g_runtime.menu.aim_percent;
    next.invert_x = g_runtime.menu.invert_x;
    next.invert_y = g_runtime.menu.invert_y;
    if (!g_runtime.can_save || !hogs_ini_validate(&next)) {
        g_runtime.error = HNR_ERROR_READ;
        g_runtime.menu.active = 1;
        g_runtime.menu.status = HM_MENU_STATUS_EDITING;
        return;
    }
    result = hogs_ini_save(g_runtime.ini_path, &next);
    if (result != HOGS_INI_OK) {
        g_runtime.error = HNR_ERROR_SAVE;
        g_runtime.menu.active = 1;
        g_runtime.menu.status = HM_MENU_STATUS_EDITING;
        return;
    }
    g_runtime.settings = next;
    g_runtime.can_save = 0;
    g_runtime.error = HNR_ERROR_NONE;
    hnr_sync_host_state();
}

int HNR_CDECL hmn_runtime_init(const char *ini_path,
                               HnrSetWaitFn set_wait_active,
                               HnrClearInputFn clear_input) {
    size_t n;
    if (!ini_path || !*ini_path || !set_wait_active || !clear_input ||
        g_runtime.initialized) return 0;
    n = strlen(ini_path);
    if (n < 3u || n >= MAX_PATH || !((ini_path[0] && ini_path[1] == ':' &&
                             (ini_path[2] == '\\' || ini_path[2] == '/')) ||
                            (ini_path[0] == '\\' && ini_path[1] == '\\'))) return 0;
    memset(&g_runtime, 0, sizeof(g_runtime));
    memcpy(g_runtime.ini_path, ini_path, n + 1u);
    g_runtime.set_wait_active = set_wait_active;
    g_runtime.clear_input = clear_input;
    g_runtime.settings = hogs_ini_defaults();
    g_runtime.initialized = 1;
    return 1;
}

int HNR_CDECL hmn_runtime_shutdown(void) {
    HnrOptionsSnapshot s;
    uint8_t *node = NULL;
    uint32_t current = 0;
    hnr_close_menu();
    if (g_runtime.row_installed) {
        uint32_t selection;
        if (!hnr_snapshot(&s, &node, &current) || node != g_runtime.options_node ||
            s.count != HNR_OPTIONS_EXTENDED_COUNT ||
            s.action_table != (uint32_t)(uintptr_t)g_action_rows ||
            s.target_table != (uint32_t)(uintptr_t)g_target_rows ||
            !hnr_region_ok(node, HNR_NODE_STRIDE, 1)) return 0;
        if (s.selection >= HNR_OPTIONS_ORIGINAL_COUNT) {
            selection = HNR_OPTIONS_ORIGINAL_COUNT - 1u;
            if (!hnr_write(node + 0x0C, &selection, sizeof(selection))) return 0;
        }
        /* Shrink the visible count first; the original tables then serve the
           first three rows without a transient count/pointer mismatch. */
        if (!hnr_write(node + 0x00, &g_runtime.original_count, sizeof(uint32_t)) ||
            !hnr_write(node + 0x18, &g_runtime.original_action_pointer, sizeof(uint32_t)) ||
            !hnr_write(node + 0x1C, &g_runtime.original_target_pointer, sizeof(uint32_t))) return 0;
    }
    if (g_runtime.wait_policy.active) hogs_wait_policy_leave(&g_runtime.wait_policy);
    g_runtime.menu.active = 0;
    hnr_sync_host_state();
    memset(&g_runtime, 0, sizeof(g_runtime));
    return 1;
}

int HNR_CDECL hmn_runtime_ui_active(void) {
    return g_runtime.menu.active != 0;
}

const HMMenuState *HNR_CDECL hmn_runtime_menu_state(void) {
    return &g_runtime.menu;
}

int HNR_CDECL hmn_action(int action) {
    HnrOptionsSnapshot s;
    int policy_action, result;
    if (!g_runtime.initialized) return 0;
    if (!g_runtime.menu.active) {
        if (action == HMN_ACTION_CONFIRM && hnr_open_menu()) return 1;
        return 0;
    }
    if (!hnr_current_options(&s) || !hmr_ready()) {
        hnr_close_menu();
        return 0;
    }
    switch (action) {
        case HMN_ACTION_CONFIRM: policy_action = HM_MENU_ACTION_CONFIRM; break;
        case HMN_ACTION_BACK: policy_action = HM_MENU_ACTION_BACK; break;
        case HMN_ACTION_UP: policy_action = HM_MENU_ACTION_UP; break;
        case HMN_ACTION_DOWN: policy_action = HM_MENU_ACTION_DOWN; break;
        case HMN_ACTION_LEFT: policy_action = HM_MENU_ACTION_LEFT; break;
        case HMN_ACTION_RIGHT: policy_action = HM_MENU_ACTION_RIGHT; break;
        default: return 0;
    }
    result = hm_menu_action(&g_runtime.menu, 1, policy_action);
    if (result == HM_MENU_RESULT_CANCEL_REQUEST) {
        g_runtime.error = HNR_ERROR_NONE;
        hnr_sync_host_state();
    } else if (result == HM_MENU_RESULT_SAVE_REQUEST) {
        hnr_finish_save();
    }
    return 1;
}

void HNR_CDECL hmn_after_frame(void) {
    HnrOptionsSnapshot s;
    if (!g_runtime.initialized) return;
    if (!g_runtime.row_installed && !g_runtime.permanent_row_failure && hmr_ready())
        (void)hnr_install_options_row();
    if (g_runtime.menu.active && (!hnr_current_options(&s) || !hmr_ready()))
        hnr_close_menu();
}

void HNR_CDECL hmn_before_present(void) {
    /* The custom UI is queued at the native sprite-batch boundary instead. */
}

void HNR_CDECL hmn_before_sprite_batch(void) {
    HnrOptionsSnapshot snapshot;
    if (!g_runtime.initialized) return;
    if (!g_runtime.menu.active) {
        if (hmr_ready() && hnr_current_options(&snapshot))
            (void)hmr_draw_options((int)snapshot.selection);
        return;
    }
    if (!hmr_ready() || !hnr_current_options(NULL)) {
        hnr_close_menu();
        return;
    }
    if (!hmr_draw_menu(&g_runtime.menu, g_runtime.error)) hnr_close_menu();
}

void HNR_CDECL hmn_wait_enter(void) {
    if (!g_runtime.initialized) return;
    hogs_wait_policy_enter(&g_runtime.wait_policy);
    hnr_sync_host_state();
}

void HNR_CDECL hmn_wait_leave(void) {
    if (!g_runtime.initialized) return;
    hogs_wait_policy_leave(&g_runtime.wait_policy);
    hnr_sync_host_state();
}

int HNR_CDECL hmn_wait_input(int native_input) {
    if (!g_runtime.initialized) return native_input == 1;
    return hogs_wait_policy_filter(&g_runtime.wait_policy, native_input);
}

const char *HNR_CDECL hmn_text_override(void *font, int text_id) {
    HnrOptionsSnapshot s;
    (void)font;
    if (text_id != 24 || !hnr_current_options(&s)) return NULL;
    return g_mouse_label;
}
