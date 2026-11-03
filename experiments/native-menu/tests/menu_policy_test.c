#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../native-menu-policy-v1/menu_policy.h"

static void move_to(HMMenuState *m, int row) {
    while (m->selected != row)
        (void)hm_menu_action(m, 1, HM_MENU_ACTION_DOWN);
}

static void test_context_and_open_copy(void) {
    HMMenuState m = {0};
    assert(!hm_menu_open(&m, 0, 12, 300, 1, 1));
    assert(!m.active && m.status == HM_MENU_STATUS_CLOSED);
    assert(hm_menu_open(&m, 1, 12, 300, 1, 0));
    assert(m.active && m.selected == HM_MENU_ROW_TURN);
    assert(m.turn_units == 12 && m.aim_percent == 300);
    assert(m.invert_x == 1 && m.invert_y == 0);
    assert(m.dirty == 0 && m.status == HM_MENU_STATUS_EDITING);
    assert(!hm_menu_open(&m, 1, 4, 100, 0, 0)); /* no nested menu */
    assert(m.turn_units == 12 && m.aim_percent == 300);
}

static void test_turn_and_aim_bounds(void) {
    HMMenuState m = {0};
    assert(hm_menu_open(&m, 1, 1, 25, 0, 0));
    assert(hm_menu_action(&m, 1, HM_MENU_ACTION_LEFT) == HM_MENU_RESULT_NONE);
    assert(m.turn_units == 1 && !m.dirty);
    for (int i = 0; i < 50; ++i)
        (void)hm_menu_action(&m, 1, HM_MENU_ACTION_RIGHT);
    assert(m.turn_units == 24 && m.dirty);
    move_to(&m, HM_MENU_ROW_AIM);
    assert(m.aim_percent == 25);
    (void)hm_menu_action(&m, 1, HM_MENU_ACTION_LEFT);
    assert(m.aim_percent == 25);
    for (int i = 0; i < 50; ++i)
        (void)hm_menu_action(&m, 1, HM_MENU_ACTION_RIGHT);
    assert(m.aim_percent == 1000);
    (void)hm_menu_action(&m, 1, HM_MENU_ACTION_RIGHT);
    assert(m.aim_percent == 1000);
}

static void test_navigation_and_invert_toggles(void) {
    HMMenuState m = {0};
    assert(hm_menu_open(&m, 1, 12, 300, 0, 1));
    (void)hm_menu_action(&m, 1, HM_MENU_ACTION_UP);
    assert(m.selected == HM_MENU_ROW_SAVE_BACK); /* wrap */
    (void)hm_menu_action(&m, 1, HM_MENU_ACTION_DOWN);
    assert(m.selected == HM_MENU_ROW_TURN);
    move_to(&m, HM_MENU_ROW_INVERT_X);
    (void)hm_menu_action(&m, 1, HM_MENU_ACTION_CONFIRM);
    assert(m.invert_x == 1 && m.dirty);
    (void)hm_menu_action(&m, 1, HM_MENU_ACTION_LEFT);
    assert(m.invert_x == 0);
    (void)hm_menu_action(&m, 1, HM_MENU_ACTION_RIGHT);
    assert(m.invert_x == 1);
    move_to(&m, HM_MENU_ROW_INVERT_Y);
    (void)hm_menu_action(&m, 1, HM_MENU_ACTION_LEFT);
    assert(m.invert_y == 0);
    (void)hm_menu_action(&m, 1, HM_MENU_ACTION_RIGHT);
    assert(m.invert_y == 1);
    (void)hm_menu_action(&m, 1, HM_MENU_ACTION_CONFIRM);
    assert(m.invert_y == 0);
}

static void test_save_and_cancel_are_requests_only(void) {
    /* The caller's live config is separate and stays untouched until it handles
       SAVE_REQUEST. CANCEL_REQUEST has no staged-value commit path. */
    struct { int turn, aim, ix, iy; } live = {12, 300, 1, 1};
    HMMenuState m = {0};
    assert(hm_menu_open(&m, 1, live.turn, live.aim, live.ix, live.iy));
    (void)hm_menu_action(&m, 1, HM_MENU_ACTION_RIGHT);
    move_to(&m, HM_MENU_ROW_AIM);
    (void)hm_menu_action(&m, 1, HM_MENU_ACTION_RIGHT);
    move_to(&m, HM_MENU_ROW_INVERT_X);
    (void)hm_menu_action(&m, 1, HM_MENU_ACTION_CONFIRM);
    move_to(&m, HM_MENU_ROW_INVERT_Y);
    (void)hm_menu_action(&m, 1, HM_MENU_ACTION_CONFIRM);
    assert(m.turn_units == 13 && m.aim_percent == 325);
    assert(m.invert_x == 0 && m.invert_y == 0 && m.dirty);
    move_to(&m, HM_MENU_ROW_SAVE_BACK);
    assert(hm_menu_action(&m, 1, HM_MENU_ACTION_CONFIRM) == HM_MENU_RESULT_SAVE_REQUEST);
    assert(!m.active && m.status == HM_MENU_STATUS_SAVE_REQUESTED);
    assert(live.turn == 12 && live.aim == 300 && live.ix == 1 && live.iy == 1);

    memset(&m, 0, sizeof(m));
    assert(hm_menu_open(&m, 1, live.turn, live.aim, live.ix, live.iy));
    (void)hm_menu_action(&m, 1, HM_MENU_ACTION_RIGHT);
    assert(hm_menu_action(&m, 1, HM_MENU_ACTION_BACK) == HM_MENU_RESULT_CANCEL_REQUEST);
    assert(!m.active && m.status == HM_MENU_STATUS_CANCELLED);
    assert(live.turn == 12 && live.aim == 300 && live.ix == 1 && live.iy == 1);
}

static void test_wrong_context_cannot_edit_or_save(void) {
    HMMenuState m = {0};
    assert(hm_menu_open(&m, 1, 12, 300, 1, 1));
    (void)hm_menu_action(&m, 1, HM_MENU_ACTION_DOWN);
    int row = m.selected, turn = m.turn_units, aim = m.aim_percent;
    assert(hm_menu_action(&m, 0, HM_MENU_ACTION_RIGHT) == HM_MENU_RESULT_NONE);
    assert(hm_menu_action(&m, 0, HM_MENU_ACTION_CONFIRM) == HM_MENU_RESULT_NONE);
    assert(m.active && m.selected == row && m.turn_units == turn && m.aim_percent == aim);
}

int main(void) {
    test_context_and_open_copy();
    test_turn_and_aim_bounds();
    test_navigation_and_invert_toggles();
    test_save_and_cancel_are_requests_only();
    test_wrong_context_cannot_edit_or_save();
    puts("PASS: native menu policy tests");
    return 0;
}
