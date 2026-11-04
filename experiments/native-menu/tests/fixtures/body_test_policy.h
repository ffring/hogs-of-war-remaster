#ifndef HOGS_BODY_BRIDGE_H
#define HOGS_BODY_BRIDGE_H
#include <stdint.h>

/* Staged policy core for a future callsite trampoline; not an installed hook. */
enum { HB_BODY_DELTA_LIMIT = 90, HB_BODY_TOKEN_SLOTS = 8 };

typedef struct {
    uintptr_t actor;
    uintptr_t pig;
    uint32_t update_serial;
    uint32_t staged_ms;
    int32_t wire_delta;
    int32_t desired_delta;
    uint8_t human_type0;
    uint8_t body_route;
    uint8_t keyboard_lr_clear;
    uint8_t network_manager_known;
    uint8_t network_inactive;
    uint8_t valid;
} HBBodyToken;

typedef struct {
    HBBodyToken slots[HB_BODY_TOKEN_SLOTS];
    uint32_t next_slot;
} HBBodyTokenRing;

typedef struct {
    uintptr_t actor;
    uintptr_t pig;
    uint32_t update_serial;
    uint32_t now_ms;
    uint8_t human_type0;
    uint8_t body_route;
    uint8_t keyboard_lr_clear;
    uint8_t network_manager_known;
    uint8_t network_inactive;
} HBBodyCall;

static inline int hb_body_clamp(int value) {
    return value < -HB_BODY_DELTA_LIMIT ? -HB_BODY_DELTA_LIMIT :
           value > HB_BODY_DELTA_LIMIT ? HB_BODY_DELTA_LIMIT : value;
}

static inline int hb_body_trunc0_div4(int64_t value) {
    return value < 0 ? (int)(-((-value) / 4)) : (int)(value / 4);
}

/* stage desired from raw mouse dx and units-per-pixel (12 means 3x the
   established 4-unit baseline). Keep wire_delta separately for stale detection. */
static inline void hb_body_token_stage(HBBodyToken *t, uintptr_t actor,
                                      uintptr_t pig, uint32_t serial, uint32_t now_ms,
                                      int raw_dx, int units_per_pixel,
                                      int wire_delta, int human_type0,
                                      int body_route, int keyboard_lr_clear,
                                      int network_manager_known, int network_inactive) {
    if (!t) return;
    t->actor=actor; t->pig=pig; t->update_serial=serial; t->staged_ms=now_ms;
    t->wire_delta=(int32_t)hb_body_clamp(wire_delta);
    t->desired_delta=(int32_t)hb_body_clamp(hb_body_trunc0_div4((int64_t)raw_dx * units_per_pixel));
    t->human_type0=(uint8_t)!!human_type0; t->body_route=(uint8_t)!!body_route;
    t->keyboard_lr_clear=(uint8_t)!!keyboard_lr_clear;
    t->network_manager_known=(uint8_t)!!network_manager_known;
    t->network_inactive=(uint8_t)!!network_inactive;
    t->valid=(uint8_t)(actor && pig && human_type0 && body_route && keyboard_lr_clear &&
                       network_manager_known && network_inactive);
}

static inline void hb_body_ring_stage(HBBodyTokenRing *ring, uintptr_t actor,
                                      uintptr_t pig, uint32_t serial, uint32_t now_ms,
                                      int raw_dx, int units_per_pixel,
                                      int wire_delta, int human_type0,
                                      int body_route, int keyboard_lr_clear,
                                      int network_manager_known, int network_inactive) {
    if (!ring) return;
    uint32_t slot = ring->next_slot++ % HB_BODY_TOKEN_SLOTS;
    hb_body_token_stage(&ring->slots[slot],actor,pig,serial,now_ms,raw_dx,units_per_pixel,
                        wire_delta,human_type0,body_route,keyboard_lr_clear,
                        network_manager_known,network_inactive);
}

/* Called by a staged trampoline before the original native yaw call. The
   caller supplies facts gathered from its current actor/controller context.
   On every rejection the native wire argument remains untouched. */
static inline int hb_body_token_apply(HBBodyToken *t, const HBBodyCall *c,
                                      int32_t *native_delta) {
    if (!t || !c || !native_delta || !t->valid) return 0;
    if (!c->actor || !c->pig || t->actor!=c->actor || t->pig!=c->pig ||
        t->update_serial!=c->update_serial || (uint32_t)(c->now_ms-t->staged_ms)>100 || !t->human_type0 || !t->body_route ||
        !t->keyboard_lr_clear || !t->network_manager_known || !t->network_inactive ||
        !c->human_type0 || !c->body_route || !c->keyboard_lr_clear ||
        !c->network_manager_known || !c->network_inactive ||
        *native_delta!=t->wire_delta) return 0;
    *native_delta=t->desired_delta;
    t->valid=0;
    return 1;
}

static inline int hb_body_ring_apply(HBBodyTokenRing *ring, const HBBodyCall *c,
                                     int32_t *native_delta) {
    if (!ring || !c || !native_delta) return 0;
    for (uint32_t i=0;i<HB_BODY_TOKEN_SLOTS;i++) {
        HBBodyToken *t=&ring->slots[i];
        if (t->valid && t->actor==c->actor && t->pig==c->pig &&
            t->update_serial==c->update_serial)
            return hb_body_token_apply(t,c,native_delta);
    }
    return 0;
}

#endif
