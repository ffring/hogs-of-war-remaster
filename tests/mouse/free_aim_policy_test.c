#include <stdio.h>
#include <string.h>
#include "button_state.h"
#include "poll_logic.h"

static int fail(const char *s) { fprintf(stderr, "FAIL: %s\n", s); return 1; }

int main(void) {
    HMButtonState b = {0};
    HMState aim = {0};

    /* Free aim maps cursor movement with no RMB held. */
    hm_motion(&aim, 8, 0, 100);
    uint16_t out = hm_read_aim(&aim, 100, 4, 0, 0, 0);
    if ((out & (HM_RIGHT | HM_L1)) != (HM_RIGHT | HM_L1) || aim.dx != 4)
        return fail("free aim without right-button hold");
    puts("PASS free aim maps delta while RMB is up");

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
