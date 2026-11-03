#ifndef HOGS_NATIVE_AXIS_BRIDGE_H
#define HOGS_NATIVE_AXIS_BRIDGE_H

/*
 * Version-guarded bridge for the Steam PC build of Hogs of War.
 * Hook point: 0x0045DB97, replacing exactly `mov esi,[esp+134h]`.
 * The callback runs on the game's existing thread and may edit only the
 * supplied axis packet and button-mask word.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#if !defined(_M_IX86) && !defined(__i386__)
#error This bridge is only valid for x86 (32-bit) Windows builds.
#endif

enum {
    HM_NATIVE_EXE_BASE = 0x00400000u,
    HM_NATIVE_EXE_TIMESTAMP = 0x3A069F75u,
    HM_NATIVE_EXE_SIZE = 0x0015E000u,
    HM_NATIVE_SITE_VA = 0x0045DB97u,
    HM_NATIVE_SITE_RVA = HM_NATIVE_SITE_VA - HM_NATIVE_EXE_BASE,
    HM_NATIVE_STOLEN_BYTES = 7
};

enum {
    HM_NATIVE_PATCH_UNCERTAIN = -1,
    HM_NATIVE_PATCH_CLEAN_FAILURE = 0,
    HM_NATIVE_PATCH_COMMITTED = 1
};

typedef void (__cdecl *HMNativeAxisCallback)(BYTE *user, DWORD *axes,
                                              DWORD *button_mask);

static const BYTE hm_native_site_original[HM_NATIVE_STOLEN_BYTES] = {
    0x8B, 0xB4, 0x24, 0x34, 0x01, 0x00, 0x00
};

static HMNativeAxisCallback volatile hm_native_callback;
static BYTE *volatile hm_native_return_address;
static BYTE *volatile hm_native_site_address;
static BYTE hm_native_original_bytes[HM_NATIVE_STOLEN_BYTES];
static volatile LONG hm_native_owned_flag;

static __declspec(naked) void __cdecl hm_native_axis_stub(void) {
    __asm {
        /* Reproduce the exact seven-byte instruction that is being replaced. */
        mov esi, dword ptr [esp+134h]
        pushfd
        pushad
        cld
        /* pushad layout: saved EDI at +0, saved ESI at +4.  The original
           entry ESP is current ESP+24h, so its [ESP+0Ch] argument is +30h. */
        mov eax, dword ptr [esp]
        mov ecx, dword ptr [esp+4]
        lea edx, dword ptr [esp+30h]
        push edx
        push ecx
        push eax
        call dword ptr [hm_native_callback]
        add esp, 0Ch
        popad
        popfd
        jmp dword ptr [hm_native_return_address]
    }
}

typedef BOOL (WINAPI *HMNativeProtectFn)(LPVOID, SIZE_T, DWORD, PDWORD);
typedef BOOL (WINAPI *HMNativeFlushFn)(HANDLE, LPCVOID, SIZE_T);

typedef struct HMNativePatchOps {
    HMNativeProtectFn protect;
    HMNativeFlushFn flush;
} HMNativePatchOps;

static BOOL WINAPI hm_native_default_protect(LPVOID p, SIZE_T n, DWORD mode,
                                             PDWORD old_mode) {
    return VirtualProtect(p, n, mode, old_mode);
}

static BOOL WINAPI hm_native_default_flush(HANDLE process, LPCVOID p, SIZE_T n) {
    return FlushInstructionCache(process, p, n);
}

static const HMNativePatchOps hm_native_default_ops = {
    hm_native_default_protect, hm_native_default_flush
};

static BOOL hm_native_make_jump(BYTE *site, uintptr_t target,
                                BYTE patch[HM_NATIVE_STOLEN_BYTES]) {
    int64_t rel = (int64_t)target - ((int64_t)(uintptr_t)site + 5);
    int32_t rel32;
    if (rel < INT32_MIN || rel > INT32_MAX) return FALSE;
    rel32 = (int32_t)rel;
    patch[0] = 0xE9;
    memcpy(patch + 1, &rel32, sizeof(rel32));
    patch[5] = 0x90;
    patch[6] = 0x90;
    return TRUE;
}

static BOOL hm_native_jump_owned(BYTE *site, uintptr_t target) {
    BYTE expected[HM_NATIVE_STOLEN_BYTES];
    return hm_native_make_jump(site, target, expected) &&
           memcmp(site, expected, sizeof(expected)) == 0;
}

/* Shared transactional writer. If rollback cannot be flushed, a stale CPU
   cache may still enter the stub, so return UNCERTAIN and retain its targets. */
static int hm_native_patch_install(BYTE *site, uintptr_t target,
                                   BYTE original[HM_NATIVE_STOLEN_BYTES],
                                   const HMNativePatchOps *ops) {
    BYTE patch[HM_NATIVE_STOLEN_BYTES];
    BYTE before[HM_NATIVE_STOLEN_BYTES];
    DWORD old_protect = 0, ignored = 0;
    HANDLE process = GetCurrentProcess();
    if (!site || !original || !ops || !ops->protect || !ops->flush ||
        !hm_native_make_jump(site, target, patch) ||
        memcmp(site, hm_native_site_original, sizeof(before)) != 0)
        return HM_NATIVE_PATCH_CLEAN_FAILURE;
    memcpy(before, site, sizeof(before));
    if (!ops->protect(site, sizeof(patch), PAGE_EXECUTE_READWRITE,
                      &old_protect)) return HM_NATIVE_PATCH_CLEAN_FAILURE;
    memcpy(site, patch, sizeof(patch));
    if (!ops->flush(process, site, sizeof(patch))) {
        memcpy(site, before, sizeof(before));
        BOOL rollback_flushed = ops->flush(process, site, sizeof(before));
        (void)ops->protect(site, sizeof(before), old_protect, &ignored);
        return rollback_flushed ? HM_NATIVE_PATCH_CLEAN_FAILURE : HM_NATIVE_PATCH_UNCERTAIN;
    }
    memcpy(original, before, sizeof(before));
    if (!ops->protect(site, sizeof(patch), old_protect, &ignored)) {
        /* Code is already patched and flushed. Keep the live callback target;
           don't turn a complete patch into an uncertain rollback. */
        return HM_NATIVE_PATCH_COMMITTED;
    }
    return HM_NATIVE_PATCH_COMMITTED;
}

static int hm_native_patch_restore(BYTE *site, uintptr_t target,
                                   const BYTE original[HM_NATIVE_STOLEN_BYTES],
                                   const HMNativePatchOps *ops) {
    BYTE patch[HM_NATIVE_STOLEN_BYTES];
    DWORD old_protect = 0, ignored = 0;
    HANDLE process = GetCurrentProcess();
    if (!site || !original || !ops || !ops->protect || !ops->flush ||
        !hm_native_make_jump(site, target, patch) ||
        memcmp(site, patch, sizeof(patch)) != 0) return HM_NATIVE_PATCH_CLEAN_FAILURE;
    if (!ops->protect(site, sizeof(patch), PAGE_EXECUTE_READWRITE,
                      &old_protect)) return HM_NATIVE_PATCH_CLEAN_FAILURE;
    memcpy(site, original, HM_NATIVE_STOLEN_BYTES);
    if (!ops->flush(process, site, HM_NATIVE_STOLEN_BYTES)) {
        memcpy(site, patch, sizeof(patch));
        BOOL patch_flushed = ops->flush(process, site, sizeof(patch));
        (void)ops->protect(site, sizeof(patch), old_protect, &ignored);
        return patch_flushed ? HM_NATIVE_PATCH_CLEAN_FAILURE : HM_NATIVE_PATCH_UNCERTAIN;
    }
    if (!ops->protect(site, HM_NATIVE_STOLEN_BYTES, old_protect, &ignored)) {
        /* The original bytes are flushed, so no stub entry can remain. */
        return HM_NATIVE_PATCH_COMMITTED;
    }
    return HM_NATIVE_PATCH_COMMITTED;
}

static BOOL hm_native_identity_fields(uintptr_t loaded_base, WORD machine,
                                      DWORD preferred_base, DWORD timestamp,
                                      DWORD image_size) {
    return loaded_base == HM_NATIVE_EXE_BASE &&
           machine == IMAGE_FILE_MACHINE_I386 &&
           preferred_base == HM_NATIVE_EXE_BASE &&
           timestamp == HM_NATIVE_EXE_TIMESTAMP &&
           image_size == HM_NATIVE_EXE_SIZE;
}

static BOOL hm_native_readable(const void *address, SIZE_T size) {
    MEMORY_BASIC_INFORMATION mbi;
    BYTE *p = (BYTE *)address;
    SIZE_T remaining = size;
    while (remaining) {
        SIZE_T got = VirtualQuery(p, &mbi, sizeof(mbi));
        DWORD protection;
        SIZE_T span;
        if (got != sizeof(mbi) || mbi.State != MEM_COMMIT ||
            (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return FALSE;
        protection = mbi.Protect & 0xFF;
        if (protection != PAGE_READONLY && protection != PAGE_READWRITE &&
            protection != PAGE_WRITECOPY && protection != PAGE_EXECUTE_READ &&
            protection != PAGE_EXECUTE_READWRITE &&
            protection != PAGE_EXECUTE_WRITECOPY) return FALSE;
        span = (SIZE_T)((BYTE *)mbi.BaseAddress + mbi.RegionSize - p);
        if (!span) return FALSE;
        if (span >= remaining) return TRUE;
        remaining -= span;
        p += span;
    }
    return TRUE;
}

static BOOL hm_native_image_matches(HMODULE exe) {
    BYTE *base = (BYTE *)exe;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS32 *nt;
    DWORD off;
    if (!exe || (uintptr_t)base != HM_NATIVE_EXE_BASE ||
        !hm_native_readable(base, sizeof(IMAGE_DOS_HEADER))) return FALSE;
    dos = (IMAGE_DOS_HEADER *)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        dos->e_lfanew > 0x1000) return FALSE;
    off = (DWORD)dos->e_lfanew;
    if (!hm_native_readable(base + off, sizeof(IMAGE_NT_HEADERS32))) return FALSE;
    nt = (IMAGE_NT_HEADERS32 *)(base + off);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) return FALSE;
    return hm_native_identity_fields((uintptr_t)base, nt->FileHeader.Machine,
        nt->OptionalHeader.ImageBase, nt->FileHeader.TimeDateStamp,
        nt->OptionalHeader.SizeOfImage);
}

/* Call only from the game's thread at a point where the target site cannot be
   executing concurrently. The callback is published before the JMP is written. */
static BOOL hm_native_bridge_owned(void);

static int hm_native_install_site(BYTE *site, HMNativeAxisCallback callback,
                                  const HMNativePatchOps *ops) {
    int result;
    if (!site || !callback || InterlockedCompareExchange(&hm_native_owned_flag, 0, 0) != 0)
        return HM_NATIVE_PATCH_CLEAN_FAILURE;
    hm_native_callback = callback;
    hm_native_return_address = site + HM_NATIVE_STOLEN_BYTES;
    result = hm_native_patch_install(site, (uintptr_t)hm_native_axis_stub,
                                     hm_native_original_bytes, ops);
    if (result == HM_NATIVE_PATCH_CLEAN_FAILURE) {
        hm_native_callback = NULL;
        hm_native_return_address = NULL;
        return HM_NATIVE_PATCH_CLEAN_FAILURE;
    }
    hm_native_site_address = site;
    /* State 3 fails closed in the caller, but retains a safe stub destination
       in case an instruction cache still contains our jump. */
    InterlockedExchange(&hm_native_owned_flag,
                        result == HM_NATIVE_PATCH_UNCERTAIN ? 3 : 2);
    return result;
}

static int hm_native_bridge_install(HMODULE exe, HMNativeAxisCallback callback) {
    BYTE *site;
    LONG state = InterlockedCompareExchange(&hm_native_owned_flag, 0, 0);
    if (state == 2) return hm_native_bridge_owned() ? HM_NATIVE_PATCH_COMMITTED : HM_NATIVE_PATCH_UNCERTAIN;
    if (state != 0) return HM_NATIVE_PATCH_UNCERTAIN;
    if (!callback || !hm_native_image_matches(exe)) return HM_NATIVE_PATCH_CLEAN_FAILURE;
    site = (BYTE *)exe + HM_NATIVE_SITE_RVA;
    if (!hm_native_readable(site, HM_NATIVE_STOLEN_BYTES)) return HM_NATIVE_PATCH_CLEAN_FAILURE;
    return hm_native_install_site(site, callback, &hm_native_default_ops);
}

static BOOL hm_native_bridge_uninstall(void) {
    LONG state = InterlockedCompareExchange(&hm_native_owned_flag, 0, 0);
    int result;
    if (state != 2) return FALSE;
    result = hm_native_patch_restore((BYTE *)hm_native_site_address,
                                     (uintptr_t)hm_native_axis_stub,
                                     hm_native_original_bytes,
                                     &hm_native_default_ops);
    if (result == HM_NATIVE_PATCH_CLEAN_FAILURE) return FALSE;
    if (result == HM_NATIVE_PATCH_UNCERTAIN) {
        InterlockedExchange(&hm_native_owned_flag, 3);
        return FALSE;
    }
    InterlockedExchange(&hm_native_owned_flag, 0);
    hm_native_callback = NULL;
    hm_native_return_address = NULL;
    hm_native_site_address = NULL;
    return TRUE;
}

static BOOL hm_native_bridge_owned(void) {
    BYTE *site;
    if (InterlockedCompareExchange(&hm_native_owned_flag, 0, 0) != 2) return FALSE;
    site = (BYTE *)hm_native_site_address;
    return site && hm_native_jump_owned(site, (uintptr_t)hm_native_axis_stub);
}

#ifdef HOGSMOUSE_TEST
static int hm_native_test_patch_install(BYTE *site, uintptr_t target,
                                        BYTE saved[HM_NATIVE_STOLEN_BYTES],
                                        const HMNativePatchOps *ops) {
    return hm_native_patch_install(site, target, saved, ops);
}
static int hm_native_test_patch_restore(BYTE *site, uintptr_t target,
                                       const BYTE saved[HM_NATIVE_STOLEN_BYTES],
                                       const HMNativePatchOps *ops) {
    return hm_native_patch_restore(site, target, saved, ops);
}
static BOOL hm_native_test_jump_owned(BYTE *site, uintptr_t target) {
    return hm_native_jump_owned(site, target);
}
static int hm_native_test_install_site(BYTE *site, HMNativeAxisCallback callback,
                                       const HMNativePatchOps *ops) {
    return hm_native_install_site(site, callback, ops);
}
static LONG hm_native_test_state(void) {
    return InterlockedCompareExchange(&hm_native_owned_flag, 0, 0);
}
#endif

#endif /* HOGS_NATIVE_AXIS_BRIDGE_H */
