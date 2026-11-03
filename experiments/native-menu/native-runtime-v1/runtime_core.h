#ifndef HOGS_NATIVE_RUNTIME_CORE_H
#define HOGS_NATIVE_RUNTIME_CORE_H

#include <stdint.h>
#include <string.h>
#include "../native-menu-policy-v1/menu_policy.h"

#define HNR_OPTIONS_TITLE_ID       17u
#define HNR_OPTIONS_FIRST_TEXT_ID  21u
#define HNR_OPTIONS_NODE_INDEX     2u
#define HNR_NODE_STRIDE            0x48u
#define HNR_OPTIONS_ORIGINAL_COUNT  3u
#define HNR_OPTIONS_EXTENDED_COUNT 4u
#ifdef HNR_TEST
#ifndef HNR_ORIGINAL_ACTION_TABLE
#define HNR_ORIGINAL_ACTION_TABLE  0x004C0FD8u
#endif
#ifndef HNR_ORIGINAL_TARGET_TABLE
#define HNR_ORIGINAL_TARGET_TABLE  0x004C1230u
#endif
#ifndef HNR_OPTIONS_ROOT_GLOBAL
#define HNR_OPTIONS_ROOT_GLOBAL    0x0051AAE0u
#endif
#ifndef HNR_CURRENT_NODE_GLOBAL
#define HNR_CURRENT_NODE_GLOBAL    0x0051AAD8u
#endif
#ifndef HNR_FRONTEND_GLOBAL
#define HNR_FRONTEND_GLOBAL       0x0051F120u
#endif
#else
#define HNR_ORIGINAL_ACTION_TABLE 0x004C0FD8u
#define HNR_ORIGINAL_TARGET_TABLE 0x004C1230u
#define HNR_OPTIONS_ROOT_GLOBAL   0x0051AAE0u
#define HNR_CURRENT_NODE_GLOBAL   0x0051AAD8u
#define HNR_FRONTEND_GLOBAL       0x0051F120u
#endif

typedef struct HnrOptionsSnapshot {
    uint32_t count;
    uint32_t title_id;
    uint32_t first_text_id;
    uint32_t selection;
    uint32_t action_table;
    uint32_t target_table;
} HnrOptionsSnapshot;

static inline int hnr_original_options_tables(const uint32_t action[3],
                                               const uint32_t target[3]) {
    static const uint32_t expected_action[3] = {2u, 1u, 1u};
    static const uint32_t expected_target[3] = {20u, 41u, 45u};
    return action && target &&
        memcmp(action, expected_action, sizeof(expected_action)) == 0 &&
        memcmp(target, expected_target, sizeof(expected_target)) == 0;
}

static inline int hnr_build_options_tables(const uint32_t action[3],
                                            const uint32_t target[3],
                                            uint32_t out_action[4],
                                            uint32_t out_target[4]) {
    if (!out_action || !out_target || !hnr_original_options_tables(action, target)) return 0;
    memcpy(out_action, action, 3u * sizeof(uint32_t));
    memcpy(out_target, target, 3u * sizeof(uint32_t));
    out_action[3] = 1u; /* ordinary selectable row */
    out_target[3] = HNR_OPTIONS_NODE_INDEX; /* selecting it stays on Options */
    return 1;
}

static inline int hnr_extended_options_tables(const uint32_t action[4],
                                               const uint32_t target[4]) {
    static const uint32_t expected_action[4] = {2u, 1u, 1u, 1u};
    static const uint32_t expected_target[4] = {20u, 41u, 45u, HNR_OPTIONS_NODE_INDEX};
    return action && target &&
        memcmp(action, expected_action, sizeof(expected_action)) == 0 &&
        memcmp(target, expected_target, sizeof(expected_target)) == 0;
}

static inline int hnr_options_base_matches(const HnrOptionsSnapshot *s,
                                            const uint32_t action[3],
                                            const uint32_t target[3]) {
    return s && s->count == HNR_OPTIONS_ORIGINAL_COUNT &&
        s->title_id == HNR_OPTIONS_TITLE_ID &&
        s->first_text_id == HNR_OPTIONS_FIRST_TEXT_ID &&
        s->action_table == HNR_ORIGINAL_ACTION_TABLE &&
        s->target_table == HNR_ORIGINAL_TARGET_TABLE &&
        s->selection < HNR_OPTIONS_ORIGINAL_COUNT &&
        hnr_original_options_tables(action, target);
}

static inline int hnr_options_extended_matches(const HnrOptionsSnapshot *s,
                                                uint32_t owned_actions,
                                                uint32_t owned_targets) {
    return s && s->count == HNR_OPTIONS_EXTENDED_COUNT &&
        s->title_id == HNR_OPTIONS_TITLE_ID &&
        s->first_text_id == HNR_OPTIONS_FIRST_TEXT_ID &&
        s->action_table == owned_actions && s->target_table == owned_targets &&
        s->selection < HNR_OPTIONS_EXTENDED_COUNT;
}

static inline int hnr_is_options_context(const HnrOptionsSnapshot *s,
                                          uint32_t current_node,
                                          uint32_t options_node,
                                          uint32_t owned_actions,
                                          uint32_t owned_targets) {
    return current_node == options_node &&
        hnr_options_extended_matches(s, owned_actions, owned_targets);
}

static inline int hnr_is_active_options_context(uint32_t frontend_active,
                                                 const HnrOptionsSnapshot *s,
                                                 uint32_t current_node,
                                                 uint32_t options_node,
                                                 uint32_t owned_actions,
                                                 uint32_t owned_targets) {
    return frontend_active != 0 &&
        hnr_is_options_context(s, current_node, options_node,
                               owned_actions, owned_targets);
}

#endif
