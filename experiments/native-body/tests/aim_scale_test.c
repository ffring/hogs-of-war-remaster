#include <limits.h>
#include <stdio.h>
#include <stdint.h>

#include "aim_scale.h"
#include "mouse_logic.h"
#include "button_state.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); return 1; \
} } while (0)

static int32_t abs_i32(int32_t v) { return v < 0 ? -v : v; }

static int count_right_pulses(int32_t percent) {
    HMAimScale scale; HMState aim = {0}; int pulses = 0;
    hm_aim_scale_init(&scale, percent);
    for (uint32_t frame = 1; frame <= 4; ++frame) {
        int32_t dx, dy;
        if (!hm_aim_scale_frame(&scale, 1, 0, &dx, &dy)) return -1;
        aim.dx = dx; aim.dy = dy; aim.last_motion_ms = frame;
        uint16_t keys = hm_read_aim(&aim, frame, 4, 0, 1, 1);
        if (keys & HM_RIGHT) ++pulses;
        if (!(keys & HM_L1) || (keys & HM_LEFT)) return -1;
    }
    return pulses;
}

int main(void) {
    HMAimScale s; int32_t x, y;
    hm_aim_scale_init(&s, 0);
    CHECK(s.percent == 300);
    CHECK(hm_aim_scale_normalize_percent(-10) == 300);
    CHECK(hm_aim_scale_normalize_percent(1001) == 1000);
    puts("PASS default gain and configured gain bounds");

    hm_aim_scale_init(&s, 150);
    CHECK(hm_aim_scale_frame(&s, 1, -1, &x, &y));
    CHECK(x == 1 && y == -1 && s.fraction_x100_x == 50 && s.fraction_x100_y == -50);
    CHECK(hm_aim_scale_frame(&s, 0, 0, &x, &y));
    CHECK(x == 0 && y == 0 && s.fraction_x100_x == 50 && s.fraction_x100_y == -50);
    CHECK(hm_aim_scale_frame(&s, 1, -1, &x, &y));
    CHECK(x == 2 && y == -2 && s.fraction_x100_x == 0 && s.fraction_x100_y == 0);
    puts("PASS fractional hundredths accumulate without producing idle motion");

    hm_aim_scale_init(&s, 150);
    CHECK(hm_aim_scale_frame(&s, 1, 0, &x, &y));
    CHECK(x == 1 && s.fraction_x100_x == 50);
    CHECK(hm_aim_scale_frame(&s, -1, 0, &x, &y));
    CHECK(x == -1 && s.fraction_x100_x == -50);
    CHECK(abs_i32(s.fraction_x100_x) < 100);
    CHECK(hm_aim_scale_frame(&s, 1, 0, &x, &y));
    CHECK(x == 1 && s.fraction_x100_x == 50);
    puts("PASS direction reversal drops the previous direction's fraction");

    hm_aim_scale_init(&s, 1000);
    CHECK(hm_aim_scale_frame(&s, INT32_MAX, INT32_MIN, &x, &y));
    CHECK(x == HM_AIM_SCALE_MAX_FRAME_DELTA && y == -HM_AIM_SCALE_MAX_FRAME_DELTA);
    CHECK(s.fraction_x100_x == 0 && s.fraction_x100_y == 0);
    CHECK(hm_aim_scale_frame(&s, 0, 0, &x, &y));
    CHECK(x == 0 && y == 0); /* No full-pixel backlog after saturation. */
    hm_aim_scale_init(&s, 300);
    CHECK(hm_aim_scale_frame(&s, INT32_MIN, INT32_MAX, &x, &y));
    CHECK(x == -HM_AIM_SCALE_MAX_FRAME_DELTA && y == HM_AIM_SCALE_MAX_FRAME_DELTA);
    CHECK(abs_i32(s.fraction_x100_x) < 100 && abs_i32(s.fraction_x100_y) < 100);
    puts("PASS INT32 extremes, output clamp, bounded fractions, and no saturation debt");

    hm_aim_scale_init(&s, 150);
    CHECK(hm_aim_scale_frame(&s, 1, 1, &x, &y));
    CHECK(s.fraction_x100_x == 50 && s.fraction_x100_y == 50);
    hm_aim_scale_clear(&s);
    CHECK(s.fraction_x100_x == 0 && s.fraction_x100_y == 0 && s.percent == 150);
    CHECK(hm_aim_scale_frame(&s, 0, 0, &x, &y));
    CHECK(x == 0 && y == 0);
    puts("PASS lifecycle reset clears fractions but retains selected gain");

    CHECK(count_right_pulses(100) == 1);
    CHECK(count_right_pulses(300) == 3);
    puts("PASS integration with hm_read_aim: 300 percent yields three pulses for same four-pixel sample");

    hm_aim_scale_init(&s, 300);
    HMState a = {0}, b = {0};
    CHECK(hm_aim_scale_frame(&s, 1, 1, &x, &y));
    a.dx = x; a.dy = y; a.last_motion_ms = 10;
    uint16_t normal = hm_read_aim(&a, 10, 1, 0, 1, 1);
    CHECK((normal & (HM_RIGHT | HM_DOWN | HM_L1)) == (HM_RIGHT | HM_DOWN | HM_L1));
    hm_aim_scale_init(&s, 300);
    CHECK(hm_aim_scale_frame(&s, 1, 1, &x, &y));
    b.dx = x; b.dy = y; b.last_motion_ms = 10;
    uint16_t inverted = hm_read_aim(&b, 10, 1, 1, 1, 1);
    CHECK((inverted & (HM_RIGHT | HM_UP | HM_L1)) == (HM_RIGHT | HM_UP | HM_L1));
    CHECK(!(inverted & HM_DOWN));
    puts("PASS X gain and Y inversion remain independent through hm_read_aim");

    HMButtonState buttons; hm_button_state_clear(&buttons);
    hm_button_event(&buttons, HM_MOUSE_LEFT, 1);
    hm_aim_scale_init(&s, 300);
    CHECK(hm_aim_scale_frame(&s, 2, -2, &x, &y));
    CHECK(hm_button_state_take_fire(&buttons) == HM_CROSS);
    CHECK(buttons.held == HM_MOUSE_LEFT);
    CHECK(hm_aim_scale_frame(&s, 0, 0, &x, &y));
    CHECK(x == 0 && y == 0 && hm_button_state_take_fire(&buttons) == HM_CROSS);
    hm_button_event(&buttons, HM_MOUSE_LEFT, 0);
    CHECK(hm_button_state_take_fire(&buttons) == 0);
    puts("PASS scaling never edits independent LMB fire/charge state");

    return 0;
}
