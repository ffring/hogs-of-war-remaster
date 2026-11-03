#include <stdio.h>
#include <string.h>
#include "button_state.h"
#include "poll_logic.h"

static int fail(const char *s) { fprintf(stderr, "FAIL: %s\n", s); return 1; }

int main(void) {
    HMButtonState b = {0};
    HMState aim = {0};

    /* Default controlled aim holds L1 for the whole RMB gesture, even while
       stationary; each read consumes motion and cannot replay a whole step. */
    hm_motion(&aim, 4, 0, 100);
    uint16_t out = hm_read_aim(&aim, 100, 4, 0, 1, 1);
    if ((out & (HM_RIGHT | HM_L1)) != (HM_RIGHT | HM_L1) || aim.dx != 0)
        return fail("RMB frame delta maps one right pulse with L1");
    out = hm_read_aim(&aim, 101, 4, 0, 1, 1);
    if (out != HM_L1) return fail("stationary held RMB lost L1 or repeated aim");
    out = hm_read_aim(&aim, 102, 4, 0, 1, 0);
    if (out != 0 || aim.remainder_dx || aim.dx) return fail("RMB release did not clear aim debt");
    puts("PASS stationary RMB holds L1; frame delta emits one pulse and release clears debt");

    memset(&aim, 0, sizeof(aim));
    hm_motion(&aim, 1, 0, 200); out = hm_read_aim(&aim, 200, 4, 0, 1, 1);
    if (out != HM_L1 || aim.remainder_dx != 1) return fail("subthreshold fraction retained");
    hm_motion(&aim, -1, 0, 201); out = hm_read_aim(&aim, 201, 4, 0, 1, 1);
    if (out != HM_L1 || aim.remainder_dx != -1) return fail("direction reversal kept stale fraction");
    hm_motion(&aim, 1, 0, 202); out = hm_read_aim(&aim, 202, 4, 0, 1, 1);
    if (out != HM_L1 || aim.remainder_dx != 1) return fail("alternating tiny deltas pulsed");
    puts("PASS tiny alternating deltas keep only signed subthreshold remainder");

    memset(&aim, 0, sizeof(aim));
    hm_motion(&aim, 32, 0, 300); out = hm_read_aim(&aim, 300, 4, 0, 1, 1);
    if ((out & (HM_RIGHT | HM_L1)) != (HM_RIGHT | HM_L1) || aim.dx || aim.remainder_dx)
        return fail("large frame generated more than one pulse or carried full-step debt");
    out = hm_read_aim(&aim, 301, 4, 0, 1, 1);
    if (out != HM_L1) return fail("large-frame direction continued while idle");
    puts("PASS large frame pulses once; idle stops; reversal does not inherit step debt");

    memset(&aim, 0, sizeof(aim));
    hm_motion(&aim, 7, 0, 350); out = hm_read_aim(&aim, 350, 4, 0, 1, 1);
    if ((out & (HM_RIGHT | HM_L1)) != (HM_RIGHT | HM_L1) || aim.remainder_dx != 3)
        return fail("fast movement did not retain only a subthreshold fraction");
    hm_motion(&aim, -1, 0, 351); out = hm_read_aim(&aim, 351, 4, 0, 1, 1);
    if (out != HM_L1 || aim.remainder_dx != -1) return fail("slow reversal inherited forward debt");
    puts("PASS fast-to-slow reversal discards old-direction fraction");

    memset(&aim, 0, sizeof(aim));
    if (!hm_normalize_require_rmb(0)) return fail("legacy free-aim INI was not normalized to RMB-required");
    hm_motion(&aim, 4, 0, 400);
    out = hm_read_aim(&aim, 400, 4, 0, hm_normalize_require_rmb(0), 0);
    if (out != 0 || aim.dx || aim.remainder_dx) return fail("legacy free-aim INI injected without RMB");
    puts("PASS legacy RequireRightButton=0 is normalized; no idle aim without RMB");

    /* Raw down is authoritative over async poll zero and stationary for >1s. */
    hm_button_event(&b, HM_MOUSE_LEFT, 1);
    for (uint32_t t = 100; t <= 1200; t += 100) {
        hm_button_poll(&b, 0);
        if (!(b.held & HM_MOUSE_LEFT)) return fail("async zero erased Raw LMB hold");
        uint16_t fire = hm_button_state_take_fire(&b);
        if (!(fire & HM_CROSS)) return fail("stationary Raw LMB charge lost");
    }
    hm_button_event(&b, HM_MOUSE_LEFT, 0);
    hm_button_poll(&b, 0);
    if (b.held || (hm_button_state_take_fire(&b) & HM_CROSS)) return fail("Raw UP did not stop fire");
    hm_button_poll(&b, HM_MOUSE_LEFT);
    if (b.held & HM_MOUSE_LEFT) return fail("async poll resurrected an event-known UP");
    puts("PASS Raw LMB hold survives async-zero polling beyond one second; actual UP stops charge");

    /* A quick click with Raw and legacy duplicates produces one pulse. */
    hm_button_state_clear(&b);
    hm_button_event(&b, HM_MOUSE_LEFT, 1); /* Raw down */
    hm_button_event(&b, HM_MOUSE_LEFT, 1); /* duplicate legacy down */
    hm_button_event(&b, HM_MOUSE_LEFT, 0); /* Raw up */
    hm_button_event(&b, HM_MOUSE_LEFT, 0); /* duplicate legacy up */
    if (!(hm_button_state_take_fire(&b) & HM_CROSS)) return fail("quick click pulse lost");
    if (hm_button_state_take_fire(&b) & HM_CROSS) return fail("duplicate event made extra click pulse");
    puts("PASS duplicate Raw/legacy click edges yield one Cross pulse");

    /* Legacy DOWN is also authoritative over a zero polling result. */
    hm_button_state_clear(&b);
    hm_button_event(&b, HM_MOUSE_LEFT, 1);
    hm_button_poll(&b, 0);
    if (!(b.held & HM_MOUSE_LEFT) || !(hm_button_state_take_fire(&b) & HM_CROSS))
        return fail("legacy LMB down ignored when async state is zero");
    puts("PASS legacy LMB DOWN remains held despite async-zero poll");

    hm_button_state_clear(&b);
    hm_button_poll(&b, HM_MOUSE_LEFT);
    hm_button_poll(&b, 0);
    if (hm_button_state_take_fire(&b) != HM_CROSS || hm_button_state_take_fire(&b) != 0)
        return fail("poll-only quick click pulse");
    puts("PASS polling fallback retains a quick LMB click until one battle read");

    /* Focus/UI/Escape reset disarms event-held state until its actual UP. */
    hm_button_state_clear(&b);
    hm_button_event(&b, HM_MOUSE_RIGHT, 1);
    hm_button_disarm(&b);
    hm_button_poll(&b, 0);
    if (hm_button_state_released(&b)) return fail("async-zero poll rearmed after event-held focus reset");
    hm_button_event(&b, HM_MOUSE_RIGHT, 0);
    if (!hm_button_state_released(&b)) return fail("actual RMB UP did not release rearm wait");
    puts("PASS focus/UI/Escape reset waits for actual UP on event-held button");

    /* Movement arbitration chooses one source; polling deltas captured while
       Raw owns input are discarded, so stale Raw cannot replay them later. */
    HMPollState p = {0};
    if (hm_poll_update(&p, 1, 10, 10, 0, 100, 1, 0, 128) != HM_POLL_BASELINE) return fail("poll baseline");
    if (hm_poll_update(&p, 1, 30, 10, 0, 110, 1, 0, 128) != HM_POLL_MOTION || p.mouse.dx != 20)
        return fail("poll motion fixture");
    int selected = hm_poll_choose_source(1, 1, 105, 110, 0, p.sample_valid, 110);
    if (selected != HM_SOURCE_RAW) return fail("fresh Raw source priority");
    hm_poll_discard_if_unselected(&p, selected);
    selected = hm_poll_choose_source(1, 1, 105, 500, 0, p.sample_valid, 500);
    if (selected != HM_SOURCE_POLL || p.mouse.dx != 0) return fail("poll replay after Raw expiry");
    puts("PASS fresh Raw takes movement priority; old poll delta is discarded before fallback");

    return 0;
}
