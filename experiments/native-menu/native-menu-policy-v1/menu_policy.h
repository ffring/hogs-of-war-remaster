#ifndef HOGS_NATIVE_MENU_POLICY_H
#define HOGS_NATIVE_MENU_POLICY_H

#include <stdint.h>

enum {
    HM_MENU_ROW_TURN = 0,
    HM_MENU_ROW_AIM = 1,
    HM_MENU_ROW_INVERT_X = 2,
    HM_MENU_ROW_INVERT_Y = 3,
    HM_MENU_ROW_SAVE_BACK = 4,
    HM_MENU_ROW_COUNT = 5
};

enum {
    HM_MENU_ACTION_NONE = 0,
    HM_MENU_ACTION_CONFIRM,
    HM_MENU_ACTION_BACK,
    HM_MENU_ACTION_UP,
    HM_MENU_ACTION_DOWN,
    HM_MENU_ACTION_LEFT,
    HM_MENU_ACTION_RIGHT
};

enum {
    HM_MENU_RESULT_NONE = 0,
    HM_MENU_RESULT_SAVE_REQUEST,
    HM_MENU_RESULT_CANCEL_REQUEST
};

enum {
    HM_MENU_STATUS_CLOSED = 0,
    HM_MENU_STATUS_EDITING,
    HM_MENU_STATUS_SAVE_REQUESTED,
    HM_MENU_STATUS_CANCELLED
};

typedef struct {
    int active;
    int selected;
    int turn_units;   /* 1..24 */
    int aim_percent;  /* 25..1000, step 25 */
    int invert_x;     /* boolean */
    int invert_y;     /* boolean */
    int dirty;
    int status;
} HMMenuState;

static inline int hm_menu_clamp(int value, int low, int high) {
    return value < low ? low : value > high ? high : value;
}

static inline int hm_menu_normalize_aim(int value) {
    value = hm_menu_clamp(value, 25, 1000);
    return ((value + 12) / 25) * 25;
}

/* Opens only in an explicitly approved game context. Values are copied into
   menu-local state; the caller's live configuration is never modified here. */
static inline int hm_menu_open(HMMenuState *menu, int context_allowed,
                               int turn_units, int aim_percent,
                               int invert_x, int invert_y) {
    if (!menu || !context_allowed || menu->active) return 0;
    menu->active = 1;
    menu->selected = HM_MENU_ROW_TURN;
    menu->turn_units = hm_menu_clamp(turn_units, 1, 24);
    menu->aim_percent = hm_menu_normalize_aim(aim_percent);
    menu->invert_x = !!invert_x;
    menu->invert_y = !!invert_y;
    menu->dirty = 0;
    menu->status = HM_MENU_STATUS_EDITING;
    return 1;
}

/* One call represents one already-debounced game action. This policy has no
   repeat timer, key polling, UI calls, or writes to the caller's settings. */
static inline int hm_menu_action(HMMenuState *menu, int context_allowed,
                                 int action) {
    if (!menu || !menu->active || menu->status != HM_MENU_STATUS_EDITING)
        return HM_MENU_RESULT_NONE;
    if (!context_allowed) return HM_MENU_RESULT_NONE;

    if (action == HM_MENU_ACTION_BACK) {
        menu->active = 0;
        menu->status = HM_MENU_STATUS_CANCELLED;
        return HM_MENU_RESULT_CANCEL_REQUEST;
    }
    if (action == HM_MENU_ACTION_UP || action == HM_MENU_ACTION_DOWN) {
        int delta = action == HM_MENU_ACTION_UP ? -1 : 1;
        menu->selected = (menu->selected + delta + HM_MENU_ROW_COUNT) % HM_MENU_ROW_COUNT;
        return HM_MENU_RESULT_NONE;
    }
    if (action == HM_MENU_ACTION_CONFIRM) {
        if (menu->selected == HM_MENU_ROW_INVERT_X) {
            menu->invert_x = !menu->invert_x;
            menu->dirty = 1;
        } else if (menu->selected == HM_MENU_ROW_INVERT_Y) {
            menu->invert_y = !menu->invert_y;
            menu->dirty = 1;
        } else if (menu->selected == HM_MENU_ROW_SAVE_BACK) {
            menu->active = 0;
            menu->status = HM_MENU_STATUS_SAVE_REQUESTED;
            return HM_MENU_RESULT_SAVE_REQUEST;
        }
        return HM_MENU_RESULT_NONE;
    }

    if (action != HM_MENU_ACTION_LEFT && action != HM_MENU_ACTION_RIGHT)
        return HM_MENU_RESULT_NONE;
    int delta = action == HM_MENU_ACTION_RIGHT ? 1 : -1;
    if (menu->selected == HM_MENU_ROW_TURN) {
        int next = hm_menu_clamp(menu->turn_units + delta, 1, 24);
        if (next != menu->turn_units) menu->dirty = 1;
        menu->turn_units = next;
    } else if (menu->selected == HM_MENU_ROW_AIM) {
        int next = hm_menu_clamp(menu->aim_percent + delta * 25, 25, 1000);
        if (next != menu->aim_percent) menu->dirty = 1;
        menu->aim_percent = next;
    } else if (menu->selected == HM_MENU_ROW_INVERT_X) {
        menu->invert_x = !menu->invert_x;
        menu->dirty = 1;
    } else if (menu->selected == HM_MENU_ROW_INVERT_Y) {
        menu->invert_y = !menu->invert_y;
        menu->dirty = 1;
    }
    return HM_MENU_RESULT_NONE;
}

#endif
