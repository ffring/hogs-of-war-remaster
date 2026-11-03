#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "../native_runtime.h"
#include "../native_runtime.c"
#include "../../native-ini-v1/native_ini.h"

enum { GLOBAL_MAP_BASE = 0x20000000u, GLOBAL_SIZE = 0x7000u,
       TABLE_BASE = 0x21000000u, TABLE_SIZE = 0x3000u,
       NODE_STRIDE_TEST = 0x48u, NODE_COUNT_TEST = 52u };

static unsigned failures, draws, options_draws, wait_transitions, input_clears;
static int ready = 1, draw_ok = 1, options_draw_ok = 1;
static int host_wait_active, draw_error, options_selection = -1;
static uint8_t *node_root;
static char ini_path[MAX_PATH];

#define CHECK(c, name) do { \
    if (!(c)) { fprintf(stderr, "FAIL %s\n", name); failures++; } \
    else printf("PASS %s\n", name); \
} while (0)

int hmr_ready(void) { return ready; }
int hmr_draw_menu(const HMMenuState *menu, int error) {
    if (!menu || !menu->active) return 0;
    draws++; draw_error = error;
    return draw_ok;
}
int hmr_draw_options(int selected) {
    options_draws++;
    options_selection = selected;
    return options_draw_ok;
}
void HNR_CDECL test_set_wait(int active) {
    host_wait_active = active;
    wait_transitions++;
}
void HNR_CDECL test_clear_input(void) { input_clears++; }

static uint8_t *options_node(void) { return node_root + 2u * NODE_STRIDE_TEST; }
static void put32(uint8_t *p, unsigned offset, uint32_t value) {
    memcpy(p + offset, &value, sizeof(value));
}
static uint32_t get32(const uint8_t *p, unsigned offset) {
    uint32_t value; memcpy(&value, p + offset, sizeof(value)); return value;
}
static void select_row(uint32_t value) { put32(options_node(), 0x0c, value); }

static int write_ini(const char *body) {
    HANDLE file = CreateFileA(ini_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written = 0, n = (DWORD)strlen(body);
    int ok = file != INVALID_HANDLE_VALUE && WriteFile(file, body, n, &written, NULL) && written == n;
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    WritePrivateProfileStringA(NULL, NULL, NULL, ini_path);
    return ok;
}

static int read_settings(HogsMouseSettings *s) {
    return hogs_ini_load(ini_path, s) == HOGS_INI_OK;
}

static int setup_engine_memory(void) {
    uint8_t *globals = (uint8_t *)VirtualAlloc((void *)(uintptr_t)GLOBAL_MAP_BASE, GLOBAL_SIZE,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    uint8_t *tables = (uint8_t *)VirtualAlloc((void *)(uintptr_t)TABLE_BASE, TABLE_SIZE,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    uint32_t *action = (uint32_t *)(uintptr_t)HNR_ORIGINAL_ACTION_TABLE;
    uint32_t *target = (uint32_t *)(uintptr_t)HNR_ORIGINAL_TARGET_TABLE;
    if (!globals || globals != (uint8_t *)(uintptr_t)GLOBAL_MAP_BASE || !tables ||
        tables != (uint8_t *)(uintptr_t)TABLE_BASE) {
        fprintf(stderr, "fixture engine memory allocation failed: globals=%p tables=%p winerr=%lu\n",
                globals, tables, (unsigned long)GetLastError());
        return 0;
    }
    node_root = (uint8_t *)VirtualAlloc(NULL, NODE_COUNT_TEST * NODE_STRIDE_TEST,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!node_root) { fprintf(stderr, "fixture node allocation failed: %lu\n", (unsigned long)GetLastError()); return 0; }
    memset(node_root, 0, NODE_COUNT_TEST * NODE_STRIDE_TEST);
    action[0] = 2; action[1] = 1; action[2] = 1;
    target[0] = 20; target[1] = 41; target[2] = 45;
    put32((uint8_t *)(uintptr_t)HNR_OPTIONS_ROOT_GLOBAL, 0, (uint32_t)(uintptr_t)node_root);
    put32((uint8_t *)(uintptr_t)HNR_CURRENT_NODE_GLOBAL, 0,
          (uint32_t)(uintptr_t)options_node());
    put32((uint8_t *)(uintptr_t)HNR_FRONTEND_GLOBAL, 0, 1);
    put32(options_node(), 0x00, 3); put32(options_node(), 0x04, 17);
    put32(options_node(), 0x08, 21); put32(options_node(), 0x0c, 1);
    put32(options_node(), 0x18, HNR_ORIGINAL_ACTION_TABLE);
    put32(options_node(), 0x1c, HNR_ORIGINAL_TARGET_TABLE);
    return 1;
}

static void test_row_and_label(void) {
    uint32_t *action, *target;
    const char *label;
    hmn_after_frame();
    action = (uint32_t *)(uintptr_t)get32(options_node(), 0x18);
    target = (uint32_t *)(uintptr_t)get32(options_node(), 0x1c);
    CHECK(get32(options_node(), 0) == 4, "Options row count extended from three to four");
    CHECK(action != (uint32_t *)(uintptr_t)HNR_ORIGINAL_ACTION_TABLE &&
          target != (uint32_t *)(uintptr_t)HNR_ORIGINAL_TARGET_TABLE,
          "row tables are cloned into runtime-owned arrays");
    CHECK(action[0] == 2 && action[1] == 1 && action[2] == 1 && action[3] == 1 &&
          target[0] == 20 && target[1] == 41 && target[2] == 45 && target[3] == 2,
          "first three rows are preserved and new row self-targets Options");
    label = hmn_text_override(NULL, 24);
    CHECK(label && label[0] == 0x2e && label[1] == 0x34 && label[2] == 0x2a &&
          label[3] == 0x38 && label[4] == 0x34 && label[5] == 0,
          "text ID 24 returns encoded MЫШЬ label bytes");
    CHECK(hmn_text_override(NULL, 23) == NULL, "other text IDs remain native");
    select_row(3);
    hmn_before_sprite_batch();
    CHECK(options_draws == 1 && options_selection == 3,
          "native Options page draw callback receives selected fourth row");
}

static void test_open_cancel_save(void) {
    HogsMouseSettings disk;
    select_row(3);
    CHECK(hmn_action(HMN_ACTION_CONFIRM) == 1 && hmn_runtime_ui_active() &&
          host_wait_active, "confirmed fourth row opens pane and pauses mouse input");
    hmn_before_sprite_batch();
    CHECK(draws == 1 && options_draws == 1,
          "settings pane draws in place of the Options list");
    CHECK(hmn_action(HMN_ACTION_RIGHT) == 1 && hmn_runtime_menu_state()->turn_units == 13,
          "pane actions edit copied settings");
    CHECK(hmn_action(HMN_ACTION_BACK) == 1 && !hmn_runtime_ui_active() && !host_wait_active,
          "Back cancels and restores input handling");
    CHECK(read_settings(&disk) && disk.native_units_per_pixel == 12 &&
          disk.aim_sensitivity_percent == 300 && disk.invert_x == 1 && disk.invert_y == 1,
          "cancel leaves INI settings unchanged");

    select_row(3); (void)hmn_action(HMN_ACTION_CONFIRM);
    (void)hmn_action(HMN_ACTION_DOWN); /* aim */
    (void)hmn_action(HMN_ACTION_RIGHT); /* 325% */
    (void)hmn_action(HMN_ACTION_DOWN);
    (void)hmn_action(HMN_ACTION_DOWN);
    (void)hmn_action(HMN_ACTION_DOWN); /* save/back */
    CHECK(hmn_runtime_menu_state()->selected == HM_MENU_ROW_SAVE_BACK,
          "keyboard navigation reaches Save and Back row");
    (void)hmn_action(HMN_ACTION_CONFIRM);
    CHECK(!hmn_runtime_ui_active() && read_settings(&disk) &&
          disk.aim_sensitivity_percent == 325 && disk.native_units_per_pixel == 12,
          "Save commits the changed setting and preserves other values");
}

static void test_read_error_and_draw_failure(void) {
    HogsMouseSettings disk;
    CHECK(write_ini("[Mouse]\r\nNativeUnitsPerPixel=broken\r\n"),
          "malformed fixture INI installed in temporary directory");
    select_row(3); (void)hmn_action(HMN_ACTION_CONFIRM);
    draws = 0; hmn_before_sprite_batch();
    CHECK(hmn_runtime_ui_active() && draws == 1 && draw_error == 1,
          "read failure keeps pane visible with error code 1");
    (void)hmn_action(HMN_ACTION_DOWN); (void)hmn_action(HMN_ACTION_DOWN);
    (void)hmn_action(HMN_ACTION_DOWN); (void)hmn_action(HMN_ACTION_DOWN);
    (void)hmn_action(HMN_ACTION_CONFIRM);
    draws = 0; hmn_before_sprite_batch();
    CHECK(hmn_runtime_ui_active() && draws == 1 && draw_error == 1,
          "read failure cannot be overwritten by Save");
    (void)hmn_action(HMN_ACTION_BACK);

    CHECK(write_ini("[Mouse]\r\nNativeUnitsPerPixel=12\r\nAimSensitivityPercent=300\r\nInvertX=1\r\nInvertY=1\r\n"),
          "valid fixture INI restored");
    select_row(3); (void)hmn_action(HMN_ACTION_CONFIRM);
    draw_ok = 0; hmn_before_sprite_batch(); draw_ok = 1;
    CHECK(!hmn_runtime_ui_active() && !host_wait_active,
          "renderer failure closes the pane and releases input suppression");
    CHECK(read_settings(&disk) && disk.native_units_per_pixel == 12,
          "renderer failure does not change INI settings");
}

static void test_wait_wrong_context_and_shutdown(void) {
    uint32_t *action = (uint32_t *)(uintptr_t)get32(options_node(), 0x18);
    uint32_t *target = (uint32_t *)(uintptr_t)get32(options_node(), 0x1c);
    hmn_wait_enter();
    CHECK(host_wait_active && hmn_wait_input(1) == 0 && hmn_wait_input(0) == 0 &&
          hmn_wait_input(1) == 1, "native wait requires release then a fresh press");
    hmn_wait_leave();
    CHECK(!host_wait_active && hmn_wait_input(1) == 1, "wait exit restores normal input path");
    put32((uint8_t *)(uintptr_t)HNR_CURRENT_NODE_GLOBAL, 0, (uint32_t)(uintptr_t)(node_root + 0x48));
    CHECK(hmn_action(HMN_ACTION_BACK) == 0, "actions on another native node pass through");
    put32((uint8_t *)(uintptr_t)HNR_CURRENT_NODE_GLOBAL, 0, (uint32_t)(uintptr_t)options_node());
    select_row(3);
    CHECK(hmn_runtime_shutdown(), "runtime shutdown restores its row safely");
    CHECK(get32(options_node(), 0) == 3 &&
          get32(options_node(), 0x18) == HNR_ORIGINAL_ACTION_TABLE &&
          get32(options_node(), 0x1c) == HNR_ORIGINAL_TARGET_TABLE,
          "shutdown restores original count and table pointers");
    CHECK(get32(options_node(), 0x0c) == 2 && action[0] == 2 && target[2] == 45,
          "shutdown clamps selection before restoring stock row count");
}

int main(void) {
    HogsMouseSettings initial = {12,300,1,1};
    char temp_dir[MAX_PATH];
    DWORD n = GetTempPathA(MAX_PATH, temp_dir);
    if (!n || n >= MAX_PATH || !GetTempFileNameA(temp_dir, "hnr", 0, ini_path)) {
        fprintf(stderr, "fixture temp path creation failed: %lu\n", (unsigned long)GetLastError()); return 9;
    }
    if (!setup_engine_memory()) return 10;
    {
        HogsIniResult init_result = hogs_ini_save(ini_path, &initial);
        if (init_result != HOGS_INI_OK) {
            fprintf(stderr, "initial temporary INI save failed: result=%d path=%s\n",
                    (int)init_result, ini_path);
            return 11;
        }
    }
    if (!hmn_runtime_init(ini_path, test_set_wait, test_clear_input)) return 12;
    test_row_and_label();
    test_open_cancel_save();
    test_read_error_and_draw_failure();
    test_wait_wrong_context_and_shutdown();
    DeleteFileA(ini_path);
    if (failures) { fprintf(stderr, "%u runtime fixture check(s) failed\n", failures); return 1; }
    printf("PASS: Windows native runtime integration fixture (%u draws, %u wait transitions, %u clears)\n",
           draws, wait_transitions, input_clears);
    return 0;
}
