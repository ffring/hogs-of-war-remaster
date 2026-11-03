#ifndef HOGS_FOCUS_MAILBOX_H
#define HOGS_FOCUS_MAILBOX_H

#include <stdint.h>

enum { HM_MAC_FOCUS_MAGIC = 0x484d4631u, HM_MAC_FOCUS_VERSION = 1u };

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t seq;
    uint32_t native_game_pid;
    uint32_t token_lo;
    uint32_t token_hi;
    uint32_t active;
    uint32_t reserved;
} HMMacFocusRecord;

_Static_assert(sizeof(HMMacFocusRecord) == 32, "focus mailbox must remain 32 bytes");

#endif
