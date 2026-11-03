#ifndef HOGS_SIGHT_TINYWIN_OPS_H
#define HOGS_SIGHT_TINYWIN_OPS_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "sight_patch_core.h"

/* These adapter functions deliberately use the core's default C ABI. Only
   their calls into Win32 APIs use the platform's WINAPI/stdcall convention. */
static int hsp_real_protect(void *ctx, void *address, size_t length,
                            unsigned new_protect, unsigned *old_protect) {
    DWORD old_mode = 0;
    BOOL okay;
    (void)ctx;
    okay = VirtualProtect(address, (SIZE_T)length, (DWORD)new_protect, &old_mode);
    if (okay && old_protect) *old_protect = (unsigned)old_mode;
    return okay != FALSE;
}

static int hsp_real_flush(void *ctx, const void *address, size_t length) {
    (void)ctx;
    return FlushInstructionCache(GetCurrentProcess(), address, (SIZE_T)length) != FALSE;
}

static inline HspOps hsp_real_ops(void) {
    HspOps ops;
    ops.protect = hsp_real_protect;
    ops.flush = hsp_real_flush;
    ops.ctx = NULL;
    return ops;
}
#endif
