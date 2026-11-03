#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../runtime_core.h"
#include "../../loading-prompt-v1/native_wait_policy.h"

static void test_row_map(void) {
    uint32_t action[3] = {2, 1, 1}, target[3] = {20, 41, 45};
    uint32_t extended_action[4], extended_target[4];
    HnrOptionsSnapshot base = {3, 17, 21, 1, HNR_ORIGINAL_ACTION_TABLE,
                               HNR_ORIGINAL_TARGET_TABLE};
    HnrOptionsSnapshot extended;
    assert(hnr_original_options_tables(action, target));
    assert(hnr_options_base_matches(&base, action, target));
    assert(hnr_build_options_tables(action, target, extended_action, extended_target));
    assert(extended_action[0] == 2 && extended_action[1] == 1 &&
           extended_action[2] == 1 && extended_action[3] == 1);
    assert(extended_target[0] == 20 && extended_target[1] == 41 &&
           extended_target[2] == 45 && extended_target[3] == HNR_OPTIONS_NODE_INDEX);
    assert(hnr_extended_options_tables(extended_action, extended_target));
    extended = (HnrOptionsSnapshot){4,17,21,3,0x12340000u,0x12340020u};
    assert(hnr_options_extended_matches(&extended, 0x12340000u, 0x12340020u));
    assert(hnr_is_options_context(&extended, 0x100000u, 0x100000u,
                                  0x12340000u, 0x12340020u));
    assert(hnr_is_active_options_context(1, &extended, 0x100000u, 0x100000u,
                                         0x12340000u, 0x12340020u));
    assert(!hnr_is_active_options_context(0, &extended, 0x100000u, 0x100000u,
                                          0x12340000u, 0x12340020u));
    assert(!hnr_is_options_context(&extended, 0x100048u, 0x100000u,
                                   0x12340000u, 0x12340020u));
    extended.selection = 4;
    assert(!hnr_options_extended_matches(&extended, 0x12340000u, 0x12340020u));
    extended.selection = 3;
    extended.target_table++;
    assert(!hnr_options_extended_matches(&extended, 0x12340000u, 0x12340020u));
    extended.target_table--;
    extended_target[3] = 3;
    assert(!hnr_extended_options_tables(extended_action, extended_target));
    target[1] = 42;
    assert(!hnr_build_options_tables(action, target, extended_action, extended_target));
    assert(!hnr_options_base_matches(&base, action, target));
}

static void test_label_encoding(void) {
    static const unsigned char encoded[] = {0x2e,0x34,0x2a,0x38,0x34,0};
    unsigned char decoded[5];
    for (unsigned i = 0; i < 5; ++i) decoded[i] = (unsigned char)(encoded[i] + 0x1f);
    assert(memcmp(decoded, "MSIWS", 5) == 0);
}

static void test_wait_gate(void) {
    HogsWaitInputPolicy p = {0,0};
    hogs_wait_policy_enter(&p);
    assert(!hogs_wait_policy_filter(&p, 1));
    assert(!hogs_wait_policy_filter(&p, 0));
    assert(hogs_wait_policy_filter(&p, 1));
    hogs_wait_policy_leave(&p);
    assert(hogs_wait_policy_filter(&p, 1));
}

int main(void) {
    test_row_map();
    test_label_encoding();
    test_wait_gate();
    puts("PASS: native runtime row/context and wait policy tests");
    return 0;
}
