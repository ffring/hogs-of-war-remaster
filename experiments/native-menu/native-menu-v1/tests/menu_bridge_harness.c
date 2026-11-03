#define HOGSMENU_TEST 1
#include "../menu_bridge.c"

#include <stdio.h>
#include <stdlib.h>

static unsigned failures;
static unsigned action_calls[7], original_calls[8], original_present_calls, original_text_calls, frame_order;
static unsigned wait_order, wait_enter_calls, wait_leave_calls, wait_input_calls;
static unsigned sprite_order, sprite_calls, sprite_callback_calls;
static int wait_input_arg, wait_input_result;
static int native_wait_value;
static int consume_action;
static int original_arg_ok, frame_args_ok, text_args_ok;
static const char *override_text;
static char original_result[] = "original text";
static char override_result[] = "mouse settings";
static int frame_result = 0x13572468;
static void *sentinel_arg;
static void *expected_font;

static void check(int ok, const char *name) {
    if (!ok) { fprintf(stderr, "FAIL %s\n", name); failures++; }
    else printf("PASS %s\n", name);
}

int HMN_CDECL hmn_action(int action) {
    if (action > 0 && action < 7) action_calls[action]++;
    return consume_action;
}
void HMN_CDECL hmn_after_frame(void) {
    if (frame_order != 1) frame_args_ok = 0;
    frame_order = 2;
}
void HMN_CDECL hmn_before_present(void) {
    if (frame_order != 0) frame_args_ok = 0;
    frame_order = 1;
}
void HMN_CDECL hmn_before_sprite_batch(void) {
    if (sprite_order != 1) frame_args_ok = 0;
    sprite_order = 2; sprite_callback_calls++;
}
void HMN_CDECL hmn_wait_enter(void) {
    if (wait_order != 0) frame_args_ok = 0;
    wait_order = 1; wait_enter_calls++;
}
void HMN_CDECL hmn_wait_leave(void) {
    if (wait_order != 2) frame_args_ok = 0;
    wait_order = 3; wait_leave_calls++;
}
int HMN_CDECL hmn_wait_input(int native_input) {
    wait_input_calls++; wait_input_arg = native_input;
    return wait_input_result;
}
const char *HMN_CDECL hmn_text_override(void *font, int text_id) {
    if (font != expected_font || (text_id != 24 && text_id != 99)) text_args_ok = 0;
    return text_id == 24 ? override_text : NULL;
}

static void HMN_CDECL orig_confirm(void *arg) {
    original_calls[0]++;
    original_arg_ok = (arg == sentinel_arg);
}
static void HMN_CDECL orig_back(void) { original_calls[1]++; }
static void HMN_CDECL orig_up(void) { original_calls[2]++; }
static void HMN_CDECL orig_down(void) { original_calls[3]++; }
static void HMN_CDECL orig_left(void) { original_calls[4]++; }
static void HMN_CDECL orig_right(void) { original_calls[5]++; }
static int HMN_CDECL orig_frame(int first, int second) {
    original_calls[6]++;
    frame_args_ok = frame_args_ok && first == 7 && second == 0x2468;
    frame_order = 1;
    return frame_result;
}
static void HMN_CDECL orig_present(void) {
    original_present_calls++;
    if (frame_order != 1) frame_args_ok = 0;
    frame_order = 2;
}
static uintptr_t HMN_CDECL orig_sprite_batch(void) {
    if (sprite_order != 0) frame_args_ok = 0;
    sprite_order = 1; sprite_calls++;
    return (uintptr_t)0x76543210u;
}
static uintptr_t HMN_CDECL orig_wait_cycle(void) {
    if (wait_order != 1) frame_args_ok = 0;
    wait_order = 2;
    return (uintptr_t)0x7f123456u;
}
static int HMN_CDECL orig_wait_input(void) { return native_wait_value; }
static const char *HMN_FASTCALL orig_lookup(void *font, void *edx, int text_id) {
    (void)edx;
    if (font != expected_font || text_id != 99) text_args_ok = 0;
    original_text_calls++;
    return original_result;
}

static uint8_t *make_image(HmnIdentity *id) {
    uint8_t *image = (uint8_t *)calloc(1, HMN_IMAGE_SIZE);
    unsigned i;
    if (!image) return NULL;
    id->loaded_base = HMN_IMAGE_BASE; id->preferred_base = HMN_PREFERRED_BASE;
    id->image_size = HMN_IMAGE_SIZE; id->timestamp = HMN_PE_TIMESTAMP;
    id->machine = HMN_MACHINE_I386;
    for (i = 0; i < HMN_CALL_COUNT; ++i)
        memcpy(image + hmn_call_rvas[i], hmn_expected_call[i], 5);
    memcpy(image + hmn_text_rva, hmn_expected_text, HMN_TEXT_BYTES);
    return image;
}

typedef struct FakeMemory {
    unsigned protect_writes, protect_restores, flushes;
    unsigned fail_protect_write_at, fail_protect_restore_at, fail_flush_at, fail_flush_at2;
} FakeMemory;
static int fake_protect(void *context, uint8_t *address, size_t length,
                        int writable, uint32_t *old_protection) {
    FakeMemory *m = (FakeMemory *)context;
    (void)address; (void)length;
    if (writable) {
        m->protect_writes++;
        if (m->fail_protect_write_at == m->protect_writes) return 0;
        *old_protection = 0x20;
    } else {
        m->protect_restores++;
        if (m->fail_protect_restore_at == m->protect_restores) return 0;
    }
    return 1;
}
static int fake_flush(void *context, const uint8_t *address, size_t length) {
    FakeMemory *m = (FakeMemory *)context;
    (void)address; (void)length;
    m->flushes++;
    return m->fail_flush_at != m->flushes && m->fail_flush_at2 != m->flushes;
}

static void test_patch_transaction(void) {
    HmnIdentity id; HmnPatchPlan plan, reverse; HmnMemoryOps ops;
    FakeMemory mem = {0}; uint8_t *image = make_image(&id);
    uint32_t hooks[HMN_PATCH_COUNT]; unsigned i; int all;
    for (i = 0; i < HMN_PATCH_COUNT; ++i) hooks[i] = 0x00500000u + 0x100u * i;
    check(image != NULL, "synthetic image allocated");
    if (!image) return;
    check(hmn_make_plan(&id, image, hooks, &plan), "exact Steam identity and all hook sites accepted");
    check(hmn_rel_target(id.loaded_base, hmn_call_rvas[0], plan.patches[0].original) == 0x0042BB00u,
          "confirm CALL resolves to original target");
    check(hmn_rel_target(id.loaded_base, hmn_call_rvas[7], plan.patches[7].original) == 0x00481C10u,
          "pre-present CALL resolves to original thunk");
    check(hmn_rel_target(id.loaded_base, hmn_call_rvas[9], plan.patches[9].original) == 0x0045D2E0u &&
          hmn_rel_target(id.loaded_base, hmn_call_rvas[10], plan.patches[10].original) == 0x00480870u,
          "wait bridge CALLs resolve to their verified original targets");
    check(hmn_rel_target(id.loaded_base, hmn_call_rvas[8], plan.patches[8].original) == 0x00418C50u,
          "sprite-batch callback site resolves to the original flush");
    {
        unsigned n; int hook_map_ok = 1;
        for (n = 0; n < HMN_CALL_COUNT; ++n) {
            if (hmn_rel_target(id.loaded_base, plan.patches[n].rva,
                               plan.patches[n].replacement) != hooks[n]) hook_map_ok = 0;
        }
        check(hook_map_ok, "every verified CALL is patched to its matching hook-table slot");
    }

    { HmnIdentity bad = id; bad.timestamp++; check(!hmn_make_plan(&bad, image, hooks, &plan), "wrong PE timestamp refused"); }
    { HmnIdentity bad = id; bad.image_size--; check(!hmn_make_plan(&bad, image, hooks, &plan), "wrong image size refused"); }
    image[hmn_call_rvas[3] + 1] ^= 1;
    check(!hmn_make_plan(&id, image, hooks, &plan), "one modified callsite refuses entire install plan");
    image[hmn_call_rvas[3] + 1] ^= 1;
    image[hmn_text_rva + 3] ^= 1;
    check(!hmn_make_plan(&id, image, hooks, &plan), "modified text-lookup prologue refuses install plan");
    image[hmn_text_rva + 3] ^= 1;
    check(hmn_make_plan(&id, image, hooks, &plan), "restored exact image plans cleanly");

    ops.context = &mem; ops.protect = fake_protect; ops.flush = fake_flush;
    mem.fail_protect_write_at = 4;
    check(!hmn_apply_plan(&plan, image, &ops), "injected protection failure aborts transaction");
    check(hmn_plan_matches(&plan, image, 0) && mem.protect_restores == 3,
          "protection failure leaves every site original and restores prior protections");

    memset(&mem, 0, sizeof(mem)); mem.fail_flush_at = 3;
    check(!hmn_apply_plan(&plan, image, &ops), "injected instruction-cache failure aborts transaction");
    check(hmn_plan_matches(&plan, image, 0), "cache-flush failure rolls every written byte back");

    memset(&mem, 0, sizeof(mem)); mem.fail_flush_at = 3; mem.fail_flush_at2 = 15;
    check(hmn_apply_plan(&plan, image, &ops) == -1,
          "commit plus rollback cache-flush failures return uncertain state");
    check(hmn_plan_matches(&plan, image, 0) && mem.flushes == 3 + HMN_PATCH_COUNT,
          "uncertain rollback restores bytes and attempts every rollback cache flush");

    memset(&mem, 0, sizeof(mem)); mem.fail_protect_restore_at = 2;
    check(hmn_apply_plan(&plan, image, &ops), "protection-restore fault keeps the committed hooks installed");
    check(hmn_plan_matches(&plan, image, 1), "restore fault never frees or strands committed hook targets");
    memset(&mem, 0, sizeof(mem));
    hmn_reverse_plan(&plan, &reverse);
    check(hmn_apply_plan(&reverse, image, &ops) && hmn_plan_matches(&plan, image, 0),
          "hooks remain reversible after a protection-restore fault");

    memset(&mem, 0, sizeof(mem));
    check(hmn_apply_plan(&plan, image, &ops), "all callsites and text hook install as one checked plan");
    check(hmn_plan_matches(&plan, image, 1), "complete installed byte image matches replacements");
    hmn_reverse_plan(&plan, &reverse);
    check(hmn_apply_plan(&reverse, image, &ops), "reverse plan uninstalls every hook");
    check(hmn_plan_matches(&plan, image, 0), "uninstall restores exact original bytes");
    all = failures == 0;
    free(image);
    if (!all) fprintf(stderr, "patch transaction tests had failures\n");
}

static void test_runtime_wrappers(void) {
    int dummy;
    expected_font = &dummy; sentinel_arg = &dummy;
    hmn_test_bind_originals(orig_confirm, orig_back, orig_up, orig_down,
                            orig_left, orig_right, orig_frame, orig_present,
                            orig_sprite_batch, orig_wait_cycle, orig_wait_input, orig_lookup);
    memset(action_calls, 0, sizeof(action_calls)); memset(original_calls, 0, sizeof(original_calls));
    original_present_calls = original_text_calls = 0;
    original_arg_ok = frame_args_ok = text_args_ok = 1; consume_action = 1;
    hmn_test_confirm_wrapper(sentinel_arg); hmn_test_back_wrapper(); hmn_test_up_wrapper();
    hmn_test_down_wrapper(); hmn_test_left_wrapper(); hmn_test_right_wrapper();
    check(original_calls[0] == 0 && original_calls[1] == 0 && original_calls[2] == 0 &&
          original_calls[3] == 0 && original_calls[4] == 0 && original_calls[5] == 0,
          "handled menu actions never invoke original handlers");
    check(action_calls[1] == 1 && action_calls[2] == 1 && action_calls[3] == 1 &&
          action_calls[4] == 1 && action_calls[5] == 1 && action_calls[6] == 1,
          "each wrapper dispatches its exact action ID");

    consume_action = 0;
    hmn_test_confirm_wrapper(sentinel_arg); hmn_test_back_wrapper(); hmn_test_up_wrapper();
    hmn_test_down_wrapper(); hmn_test_left_wrapper(); hmn_test_right_wrapper();
    check(original_calls[0] == 1 && original_arg_ok && original_calls[1] == 1 &&
          original_calls[2] == 1 && original_calls[3] == 1 && original_calls[4] == 1 &&
          original_calls[5] == 1, "unhandled actions forward exactly once and confirm preserves its argument");

    frame_order = 0; frame_args_ok = 1;
    check(hmn_test_frame_wrapper(7, 0x2468) == frame_result && frame_order == 2 &&
          original_calls[6] == 1 && frame_args_ok,
          "maintenance wrapper forwards both arguments, preserves return value, then calls after-frame callback");
    frame_order = 0; hmn_test_present_wrapper();
    check(frame_order == 2 && original_present_calls == 1,
          "pre-present callback runs before original present thunk");

    sprite_order = sprite_calls = sprite_callback_calls = 0; frame_args_ok = 1;
    check(hmn_test_sprite_batch_wrapper() == (uintptr_t)0x76543210u &&
          sprite_order == 2 && sprite_calls == 1 && sprite_callback_calls == 1 && frame_args_ok,
          "sprite callback runs after original flush and wrapper preserves its EAX result");

    wait_order = 0; frame_args_ok = 1;
    wait_enter_calls = wait_leave_calls = 0;
    check(hmn_test_wait_cycle_wrapper() == (uintptr_t)0x7f123456u &&
          wait_order == 3 && wait_enter_calls == 1 && wait_leave_calls == 1 && frame_args_ok,
          "wait gate enters before original, leaves after, and preserves original EAX result");
    wait_input_calls = 0; wait_input_result = 0x1234; native_wait_value = 1;
    check(hmn_test_wait_input_wrapper() == 0x1234 && wait_input_calls == 1 && wait_input_arg == 1,
          "wait-input wrapper forwards native AL=1 to policy and returns policy result");
    wait_input_result = 0;
    check(hmn_test_wait_input_wrapper() == 0 && wait_input_calls == 2 && wait_input_arg == 1,
          "wait-input wrapper also preserves policy suppression when native input is present");
    native_wait_value = 0; wait_input_result = 1;
    check(hmn_test_wait_input_wrapper() == 1 && wait_input_calls == 3 && wait_input_arg == 0,
          "wait-input wrapper forwards native AL=0 and returns fresh-input policy decision");
    native_wait_value = 0x100; wait_input_result = 0;
    check(hmn_test_wait_input_wrapper() == 0 && wait_input_calls == 4 && wait_input_arg == 0,
          "wait-input wrapper follows retail AL semantics when upper EAX bits are set");

    text_args_ok = 1; override_text = override_result;
    check(hmn_test_lookup_wrapper(expected_font, NULL, 24) == override_result &&
          original_text_calls == 0 && text_args_ok, "text override returns directly without calling original");
    override_text = NULL;
    check(hmn_test_lookup_wrapper(expected_font, NULL, 99) == original_result &&
          original_text_calls == 1 && text_args_ok,
          "NULL text override forwards font, EDX, and ID to original lookup once");

    {
        unsigned old_action = action_calls[HMN_ACTION_CONFIRM];
        unsigned old_original = original_calls[0];
        unsigned old_after = original_calls[6];
        unsigned old_wait_enter = wait_enter_calls, old_wait_leave = wait_leave_calls;
        unsigned old_wait_policy = wait_input_calls, old_text_original = original_text_calls;
        hmn_test_set_ui_enabled(0);
        consume_action = 1;
        hmn_test_confirm_wrapper(sentinel_arg);
        check(action_calls[HMN_ACTION_CONFIRM] == old_action && original_calls[0] == old_original + 1,
              "degraded bridge forwards confirm without invoking custom UI");
        frame_order = 0; hmn_test_frame_wrapper(7, 0x2468);
        check(original_calls[6] == old_after + 1 && frame_order == 1,
              "degraded bridge forwards frame and skips custom maintenance");
        hmn_test_wait_cycle_wrapper();
        check(wait_enter_calls == old_wait_enter && wait_leave_calls == old_wait_leave,
              "degraded bridge forwards conditional wait without custom wait callbacks");
        native_wait_value = 1; wait_input_result = 0;
        check(hmn_test_wait_input_wrapper() == 1 && wait_input_calls == old_wait_policy,
              "degraded bridge passes native wait input through unchanged");
        override_text = override_result;
        check(hmn_test_lookup_wrapper(expected_font, NULL, 24) == original_result &&
              original_text_calls == old_text_original + 1,
              "degraded bridge bypasses text override but keeps original text lookup");
        hmn_test_set_ui_enabled(1);
    }
}

int main(void) {
    test_patch_transaction();
    test_runtime_wrappers();
    if (failures) { fprintf(stderr, "%u test(s) failed\n", failures); return 1; }
    puts("All HogsMenuBridge synthetic tests passed.");
    return 0;
}
