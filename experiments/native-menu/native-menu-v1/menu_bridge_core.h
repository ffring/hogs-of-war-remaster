#ifndef HOGS_MENU_BRIDGE_CORE_H
#define HOGS_MENU_BRIDGE_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define HMN_IMAGE_BASE       0x00400000u
#define HMN_IMAGE_SIZE       0x0015E000u
#define HMN_PE_TIMESTAMP     0x3A069F75u
#define HMN_MACHINE_I386     0x014Cu
#define HMN_PREFERRED_BASE   0x00400000u
#define HMN_CALL_COUNT       11u
#define HMN_TEXT_BYTES       7u
#define HMN_PATCH_COUNT      (HMN_CALL_COUNT + 1u)
#define HMN_MAX_PATCH_BYTES  7u

enum HmnHookIndex {
    HMN_HOOK_CONFIRM = 0,
    HMN_HOOK_BACK,
    HMN_HOOK_UP,
    HMN_HOOK_DOWN,
    HMN_HOOK_LEFT,
    HMN_HOOK_RIGHT,
    HMN_HOOK_AFTER_FRAME,
    HMN_HOOK_BEFORE_PRESENT,
    HMN_HOOK_SPRITE_BATCH,
    HMN_HOOK_WAIT_CYCLE,
    HMN_HOOK_WAIT_INPUT,
    HMN_HOOK_TEXT
};

typedef struct HmnIdentity {
    uint32_t loaded_base;
    uint32_t preferred_base;
    uint32_t image_size;
    uint32_t timestamp;
    uint16_t machine;
} HmnIdentity;

typedef struct HmnPatch {
    uint32_t rva;
    uint8_t length;
    uint8_t original[HMN_MAX_PATCH_BYTES];
    uint8_t replacement[HMN_MAX_PATCH_BYTES];
} HmnPatch;

typedef struct HmnPatchPlan {
    HmnPatch patches[HMN_PATCH_COUNT];
    uint32_t loaded_base;
    uint32_t image_size;
} HmnPatchPlan;

/* Protect makes one range writable and returns its previous protection. */
typedef int (*HmnProtectFn)(void *context, uint8_t *address, size_t length,
                            int writable, uint32_t *old_protection);
typedef int (*HmnFlushFn)(void *context, const uint8_t *address, size_t length);
typedef struct HmnMemoryOps {
    void *context;
    HmnProtectFn protect;
    HmnFlushFn flush;
} HmnMemoryOps;

static const uint32_t hmn_call_rvas[HMN_CALL_COUNT] = {
    0x000183ABu, /* confirm, 0x4183AB -> 0x42BB00 */
    0x000183BBu, /* back, 0x4183BB -> 0x42D790 */
    0x000183C8u, /* up, 0x4183C8 -> 0x42A6B0 */
    0x000183D5u, /* down, 0x4183D5 -> 0x42A820 */
    0x000183E2u, /* left, 0x4183E2 -> 0x42AB40 */
    0x000183EFu, /* right, 0x4183EF -> 0x42A9A0 */
    0x00027654u, /* maintenance, 0x427654 -> 0x430740 */
    0x00018897u, /* pre-present, 0x418897 -> 0x481C10 */
    0x0001847Du, /* native sprite batch, 0x41847D -> 0x418C50 */
    0x0007E2D2u, /* input wait cycle, 0x47E2D2 -> 0x45D2E0 */
    0x0005D4E0u  /* wait input predicate, 0x45D4E0 -> 0x480870 */
};

static const uint32_t hmn_target_rvas[HMN_CALL_COUNT] = {
    0x0002BB00u, 0x0002D790u, 0x0002A6B0u, 0x0002A820u,
    0x0002AB40u, 0x0002A9A0u, 0x00030740u, 0x00081C10u,
    0x00018C50u, 0x0005D2E0u, 0x00080870u
};

static const uint8_t hmn_expected_call[HMN_CALL_COUNT][5] = {
    {0xE8,0x50,0x37,0x01,0x00}, {0xE8,0xD0,0x53,0x01,0x00},
    {0xE8,0xE3,0x22,0x01,0x00}, {0xE8,0x46,0x24,0x01,0x00},
    {0xE8,0x59,0x27,0x01,0x00}, {0xE8,0xAC,0x25,0x01,0x00},
    {0xE8,0xE7,0x90,0x00,0x00}, {0xE8,0x74,0x93,0x06,0x00},
    {0xE8,0xCE,0x07,0x00,0x00}, {0xE8,0x09,0xF0,0xFD,0xFF},
    {0xE8,0x8B,0x33,0x02,0x00}
};

static const uint32_t hmn_text_rva = 0x00031680u; /* 0x431680 */
static const uint8_t hmn_expected_text[HMN_TEXT_BYTES] = {
    0x8B,0x41,0x08,0x8B,0x54,0x24,0x04
};

static inline int hmn_identity_supported(const HmnIdentity *id) {
    return id && id->loaded_base == HMN_IMAGE_BASE &&
        id->preferred_base == HMN_PREFERRED_BASE &&
        id->image_size == HMN_IMAGE_SIZE && id->timestamp == HMN_PE_TIMESTAMP &&
        id->machine == HMN_MACHINE_I386;
}

static inline int hmn_range_ok(uint32_t image_size, uint32_t rva, uint32_t length) {
    return rva <= image_size && length <= image_size - rva;
}

static inline uint32_t hmn_rel_target(uint32_t base, uint32_t site_rva,
                                      const uint8_t call[5]) {
    int32_t displacement;
    memcpy(&displacement, call + 1, sizeof(displacement));
    return base + site_rva + 5u + (uint32_t)displacement;
}

static inline void hmn_make_rel_call(uint8_t out[5], uint32_t base,
                                     uint32_t site_rva, uint32_t target) {
    int32_t displacement = (int32_t)(target - (base + site_rva + 5u));
    out[0] = 0xE8;
    memcpy(out + 1, &displacement, sizeof(displacement));
}

static inline void hmn_make_rel_jump(uint8_t out[HMN_TEXT_BYTES], uint32_t base,
                                     uint32_t site_rva, uint32_t target) {
    int32_t displacement = (int32_t)(target - (base + site_rva + 5u));
    out[0] = 0xE9;
    memcpy(out + 1, &displacement, sizeof(displacement));
    out[5] = 0x90;
    out[6] = 0x90;
}

static inline int hmn_make_plan(const HmnIdentity *id, const uint8_t *image,
                                const uint32_t hook_addresses[HMN_PATCH_COUNT],
                                HmnPatchPlan *plan) {
    unsigned i;
    if (!hmn_identity_supported(id) || !image || !hook_addresses || !plan) return 0;
    memset(plan, 0, sizeof(*plan));
    plan->loaded_base = id->loaded_base;
    plan->image_size = id->image_size;
    for (i = 0; i < HMN_CALL_COUNT; ++i) {
        HmnPatch *p = &plan->patches[i];
        uint32_t current_target;
        if (!hmn_range_ok(id->image_size, hmn_call_rvas[i], 5u)) return 0;
        p->rva = hmn_call_rvas[i]; p->length = 5;
        memcpy(p->original, image + p->rva, 5);
        if (memcmp(p->original, hmn_expected_call[i], 5) != 0) return 0;
        current_target = hmn_rel_target(id->loaded_base, p->rva, p->original);
        if (current_target != id->loaded_base + hmn_target_rvas[i]) return 0;
        hmn_make_rel_call(p->replacement, id->loaded_base, p->rva,
                          hook_addresses[i]);
    }
    {
        HmnPatch *p = &plan->patches[HMN_CALL_COUNT];
        if (!hmn_range_ok(id->image_size, hmn_text_rva, HMN_TEXT_BYTES)) return 0;
        p->rva = hmn_text_rva; p->length = HMN_TEXT_BYTES;
        memcpy(p->original, image + p->rva, HMN_TEXT_BYTES);
        if (memcmp(p->original, hmn_expected_text, HMN_TEXT_BYTES) != 0) return 0;
        hmn_make_rel_jump(p->replacement, id->loaded_base, p->rva,
                          hook_addresses[HMN_HOOK_TEXT]);
    }
    return 1;
}

static inline int hmn_plan_matches(const HmnPatchPlan *plan, const uint8_t *image,
                                   int installed) {
    unsigned i;
    if (!plan || !image) return 0;
    for (i = 0; i < HMN_PATCH_COUNT; ++i) {
        const HmnPatch *p = &plan->patches[i];
        const uint8_t *expected = installed ? p->replacement : p->original;
        if (!hmn_range_ok(plan->image_size, p->rva, p->length) ||
            memcmp(image + p->rva, expected, p->length) != 0) return 0;
    }
    return 1;
}

static inline void hmn_reverse_plan(const HmnPatchPlan *src, HmnPatchPlan *dst) {
    unsigned i;
    *dst = *src;
    for (i = 0; i < HMN_PATCH_COUNT; ++i) {
        uint8_t tmp[HMN_MAX_PATCH_BYTES];
        memcpy(tmp, dst->patches[i].original, HMN_MAX_PATCH_BYTES);
        memcpy(dst->patches[i].original, dst->patches[i].replacement, HMN_MAX_PATCH_BYTES);
        memcpy(dst->patches[i].replacement, tmp, HMN_MAX_PATCH_BYTES);
    }
}

/* Returns 1 when committed, 0 after a clean failure/rollback, and -1 when the
   bytes were restored but one or more rollback cache flushes failed. In the
   last case callers must retain every hook target/trampoline and disable UI. */
static inline int hmn_apply_plan(const HmnPatchPlan *plan, uint8_t *image,
                                 const HmnMemoryOps *ops) {
    uint32_t old[HMN_PATCH_COUNT];
    unsigned protected_count = 0, written = 0, i;
    int committed = 0, uncertain_rollback = 0;
    if (!plan || !image || !ops || !ops->protect || !ops->flush ||
        !hmn_plan_matches(plan, image, 0)) return 0;
    for (i = 0; i < HMN_PATCH_COUNT; ++i) {
        HmnPatch *p = (HmnPatch *)&plan->patches[i];
        if (!ops->protect(ops->context, image + p->rva, p->length, 1, &old[i]))
            goto restore_protection;
        protected_count++;
    }
    for (i = 0; i < HMN_PATCH_COUNT; ++i) {
        const HmnPatch *p = &plan->patches[i];
        memcpy(image + p->rva, p->replacement, p->length);
        written++;
    }
    for (i = 0; i < HMN_PATCH_COUNT; ++i) {
        const HmnPatch *p = &plan->patches[i];
        if (!ops->flush(ops->context, image + p->rva, p->length)) goto rollback_bytes;
    }
    committed = 1;
    goto restore_protection;

rollback_bytes:
    for (i = 0; i < written; ++i) {
        const HmnPatch *p = &plan->patches[i];
        memcpy(image + p->rva, p->original, p->length);
    }
    for (i = 0; i < written; ++i) {
        const HmnPatch *p = &plan->patches[i];
        if (!ops->flush(ops->context, image + p->rva, p->length))
            uncertain_rollback = 1;
    }
restore_protection:
    while (protected_count) {
        --protected_count;
        if (!ops->protect(ops->context,
                          image + plan->patches[protected_count].rva,
                          plan->patches[protected_count].length, 0,
                          &old[protected_count])) {
            /* The complete patch remains installed; callers must retain all
               hook/trampoline storage even if a page stays writable. */
        }
    }
    return uncertain_rollback ? -1 : committed;
}

#endif
