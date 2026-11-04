#ifndef HOGS_LINEAR_AIM_WRAPPER_H
#define HOGS_LINEAR_AIM_WRAPPER_H

#include <stdint.h>
#include "linear_aim_policy.h"

enum {
    HML_MODE_AIM = 14,
    HML_MODE_AIM_ALT = 17,
    HML_WEAPON_MAX_VERIFIED = 80,
    HML_WEAPON_ZOOM_SPECIAL = 11,
    HML_WEAPON_ALT_CAMERA_SPECIAL = 64
};
enum { HML_DIR_MASK = 0x0Fu, HML_CONTROLLER_HUMAN = 0, HML_CONTROLLER_QUEUE = 1 };
enum { HML_NOT_INTERCEPTED = 0, HML_AIM_APPLIED = 1, HML_ORIGINAL_ONLY = 2 };

typedef struct {
    int mode, weapon, controller_kind;
    uint32_t *axes_word; /* actor+0x444 in the real handler */
    uint32_t *button_mask;
    void *opaque;
    int (*context_unchanged)(void *opaque); /* includes supported pig vtable slot */
    int32_t (*original_handler)(void *opaque);
    void (*pitch_delta)(void *opaque, int32_t delta); /* native 0x46A7F0 */
    void (*yaw_delta)(void *opaque, int32_t delta);   /* verified pig slot +0x20 */
    int32_t original_result;
} HMLCall;

static inline int hml_mode_supported(int mode) {
    return mode==HML_MODE_AIM || mode==HML_MODE_AIM_ALT;
}

static inline int hml_weapon_supported(int weapon) {
    return weapon>=1 && weapon<=HML_WEAPON_MAX_VERIFIED &&
        weapon!=HML_WEAPON_ZOOM_SPECIAL && weapon!=HML_WEAPON_ALT_CAMERA_SPECIAL;
}

/* The callback must check the exact active-human actor/camera/pig identity,
   foreground/focus/UI gates, and pig vtable+0x20 == retail 0x0046AF30. */
static inline int hml_dispatch(HMLCall *call, HMLAxisPacket packet) {
    if (!call || !call->axes_word || !call->button_mask || !call->original_handler ||
        !call->context_unchanged || !call->pitch_delta || !call->yaw_delta ||
        !hml_mode_supported(call->mode) || !hml_weapon_supported(call->weapon) ||
        (call->controller_kind != HML_CONTROLLER_HUMAN &&
         call->controller_kind != HML_CONTROLLER_QUEUE) ||
        (*call->button_mask & HML_DIR_MASK) != 0 || !call->context_unchanged(call->opaque))
        return HML_NOT_INTERCEPTED;

    const uint32_t original = *call->axes_word;
    const uint32_t aim_word = hml_pack(packet);
    if ((original & 0xFFFFu) != aim_word || aim_word == 0) return HML_NOT_INTERCEPTED;

    /* Hide only the two mouse axes while the game's regular handler runs, so
       it still processes keyboard edges, weapon changes and other state. */
    *call->axes_word = original & 0xFFFF0000u;
    call->original_result = call->original_handler(call->opaque);
    if ((*call->axes_word & 0xFFFFu) != 0) return HML_ORIGINAL_ONLY;
    /* Keep any changes the original handler made to unrelated upper axes. */
    *call->axes_word = (*call->axes_word & 0xFFFF0000u) | (original & 0xFFFFu);
    if (!call->context_unchanged(call->opaque)) return HML_ORIGINAL_ONLY;

    const int32_t yaw = hml_retail_half(packet.x);
    const int32_t pitch = hml_retail_half(packet.y);
    if (pitch) {
        call->pitch_delta(call->opaque, pitch);
        /* A native sink may indirectly advance an actor/camera transition.
           Never issue the second axis against a context that changed meanwhile. */
        if (!call->context_unchanged(call->opaque)) return HML_ORIGINAL_ONLY;
    }
    if (yaw) call->yaw_delta(call->opaque, yaw);
    return HML_AIM_APPLIED;
}

#endif
