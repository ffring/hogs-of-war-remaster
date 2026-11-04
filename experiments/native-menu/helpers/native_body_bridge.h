#ifndef HOGS_NATIVE_BODY_BRIDGE_H
#define HOGS_NATIVE_BODY_BRIDGE_H

/* Steam PC body-yaw bridge: stages a guarded argument edit immediately before
   the retail pig vtable call. It never writes the pig/camera angle directly. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#if !defined(_M_IX86) && !defined(__i386__)
#error This bridge is only valid for x86 (32-bit) Windows builds.
#endif

enum {
    HM_BODY_EXE_BASE = 0x00400000u,
    HM_BODY_EXE_TIMESTAMP = 0x3A069F75u,
    HM_BODY_EXE_SIZE = 0x0015E000u,
    HM_BODY_SITE_VA = 0x00492D9Au,
    HM_BODY_SITE_RVA = HM_BODY_SITE_VA - HM_BODY_EXE_BASE,
    HM_BODY_STOLEN_BYTES = 7,
    HM_BODY_CONTINUE_VA = 0x00492DA4u,
    HM_BODY_UNCERTAIN = -1,
    HM_BODY_CLEAN_FAILURE = 0,
    HM_BODY_COMMITTED = 1
};

typedef void (__cdecl *HMNativeBodyCallback)(BYTE *actor, BYTE *pig,
                                               int32_t *native_delta);

static const BYTE hm_body_site_original[HM_BODY_STOLEN_BYTES] = {
    0x8B, 0x39, 0x2B, 0xC2, 0xD1, 0xF8, 0x50
};
static const BYTE hm_body_following_original[9] = {
    0xFF, 0x57, 0x20, 0x8B, 0x86, 0x40, 0x04, 0x00, 0x00
};
static HMNativeBodyCallback volatile hm_body_callback;
static BYTE *volatile hm_body_site_address;
static BYTE hm_body_original_bytes[HM_BODY_STOLEN_BYTES];
static volatile LONG hm_body_state; /* 0 absent, 2 owned, 3 uncertain/forward-only */
#ifdef HOGSMOUSE_TEST
static volatile DWORD hm_body_test_return_address;
static volatile DWORD hm_body_test_expected_method_esp;
#endif

static __declspec(naked) void __cdecl hm_body_yaw_stub(void) {
    __asm {
        /* Exact stolen instructions: EDI=vtable, EAX=wire delta / 2, push arg. */
        mov edi, dword ptr [ecx]
        sub eax, edx
        sar eax, 1
        push eax
        pushfd
        pushad
        cld
        /* Current ESP after pushfd+pushad: saved ESI +4, ECX +18h,
           original 32-bit yaw argument +24h. */
        cmp dword ptr [hm_body_callback], 0
        je body_restore
        mov eax, dword ptr [esp+4]
        mov ecx, dword ptr [esp+18h]
        lea edx, dword ptr [esp+24h]
        push edx
        push ecx
        push eax
        call dword ptr [hm_body_callback]
        add esp, 0Ch
    body_restore:
        popad
        popfd
#ifdef HOGSMOUSE_TEST
        push dword ptr [hm_body_test_return_address]
#else
        push 00492DA4h
#endif
        jmp dword ptr [edi+20h]
    }
}

typedef BOOL (WINAPI *HMBodyProtectFn)(LPVOID, SIZE_T, DWORD, PDWORD);
typedef BOOL (WINAPI *HMBodyFlushFn)(HANDLE, LPCVOID, SIZE_T);
typedef struct HMBodyPatchOps {
    HMBodyProtectFn protect;
    HMBodyFlushFn flush;
} HMBodyPatchOps;

static BOOL WINAPI hm_body_default_protect(LPVOID p, SIZE_T n, DWORD mode,
                                            PDWORD old_mode) {
    return VirtualProtect(p,n,mode,old_mode);
}
static BOOL WINAPI hm_body_default_flush(HANDLE process, LPCVOID p, SIZE_T n) {
    return FlushInstructionCache(process,p,n);
}
static const HMBodyPatchOps hm_body_default_ops={hm_body_default_protect,hm_body_default_flush};

static BOOL hm_body_make_jump(BYTE *site, uintptr_t target,
                              BYTE patch[HM_BODY_STOLEN_BYTES]) {
    int64_t rel=(int64_t)target-((int64_t)(uintptr_t)site+5);
    int32_t rel32;
    if (rel<INT32_MIN || rel>INT32_MAX) return FALSE;
    rel32=(int32_t)rel;
    patch[0]=0xE9; memcpy(patch+1,&rel32,4); patch[5]=0x90; patch[6]=0x90;
    return TRUE;
}
static BOOL hm_body_jump_owned(BYTE *site, uintptr_t target) {
    BYTE expected[HM_BODY_STOLEN_BYTES];
    return hm_body_make_jump(site,target,expected) &&
           memcmp(site,expected,sizeof(expected))==0;
}
static int hm_body_patch_install(BYTE *site, uintptr_t target,
                                 BYTE saved[HM_BODY_STOLEN_BYTES],
                                 const HMBodyPatchOps *ops) {
    BYTE patch[HM_BODY_STOLEN_BYTES], before[HM_BODY_STOLEN_BYTES];
    DWORD old=0,ignored=0; HANDLE process=GetCurrentProcess();
    if (!site || !saved || !ops || !ops->protect || !ops->flush ||
        !hm_body_make_jump(site,target,patch) ||
        memcmp(site,hm_body_site_original,sizeof(before))!=0)
        return HM_BODY_CLEAN_FAILURE;
    memcpy(before,site,sizeof(before));
    if (!ops->protect(site,sizeof(patch),PAGE_EXECUTE_READWRITE,&old))
        return HM_BODY_CLEAN_FAILURE;
    memcpy(site,patch,sizeof(patch));
    if (!ops->flush(process,site,sizeof(patch))) {
        memcpy(site,before,sizeof(before));
        BOOL rollback_flushed=ops->flush(process,site,sizeof(before));
        (void)ops->protect(site,sizeof(before),old,&ignored);
        return rollback_flushed ? HM_BODY_CLEAN_FAILURE : HM_BODY_UNCERTAIN;
    }
    memcpy(saved,before,sizeof(before));
    /* Patch bytes are live and flushed; failure to restore protection does not
       invalidate the stub, so retain it and report committed. */
    (void)ops->protect(site,sizeof(patch),old,&ignored);
    return HM_BODY_COMMITTED;
}
static int hm_body_patch_restore(BYTE *site, uintptr_t target,
                                 const BYTE original[HM_BODY_STOLEN_BYTES],
                                 const HMBodyPatchOps *ops) {
    BYTE patch[HM_BODY_STOLEN_BYTES]; DWORD old=0,ignored=0;
    HANDLE process=GetCurrentProcess();
    if (!site || !original || !ops || !ops->protect || !ops->flush ||
        !hm_body_make_jump(site,target,patch) || memcmp(site,patch,sizeof(patch))!=0)
        return HM_BODY_CLEAN_FAILURE;
    if (!ops->protect(site,sizeof(patch),PAGE_EXECUTE_READWRITE,&old))
        return HM_BODY_CLEAN_FAILURE;
    memcpy(site,original,HM_BODY_STOLEN_BYTES);
    if (!ops->flush(process,site,HM_BODY_STOLEN_BYTES)) {
        memcpy(site,patch,sizeof(patch));
        BOOL patch_flushed=ops->flush(process,site,sizeof(patch));
        (void)ops->protect(site,sizeof(patch),old,&ignored);
        return patch_flushed ? HM_BODY_CLEAN_FAILURE : HM_BODY_UNCERTAIN;
    }
    (void)ops->protect(site,HM_BODY_STOLEN_BYTES,old,&ignored);
    return HM_BODY_COMMITTED;
}
static BOOL hm_body_readable(const void *address, SIZE_T size) {
    MEMORY_BASIC_INFORMATION mbi; BYTE *p=(BYTE *)address; SIZE_T remain=size;
    while (remain) {
        if (VirtualQuery(p,&mbi,sizeof(mbi))!=sizeof(mbi) || mbi.State!=MEM_COMMIT ||
            (mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
        DWORD prot=mbi.Protect&0xFF;
        if (prot!=PAGE_READONLY && prot!=PAGE_READWRITE && prot!=PAGE_WRITECOPY &&
            prot!=PAGE_EXECUTE_READ && prot!=PAGE_EXECUTE_READWRITE &&
            prot!=PAGE_EXECUTE_WRITECOPY) return FALSE;
        uintptr_t end=(uintptr_t)mbi.BaseAddress+mbi.RegionSize;
        if (end<=(uintptr_t)p) return FALSE;
        SIZE_T span=(SIZE_T)(end-(uintptr_t)p);
        if (span>=remain) return TRUE;
        remain-=span; p+=span;
    }
    return TRUE;
}
static BOOL hm_body_image_matches(HMODULE exe) {
    BYTE *base=(BYTE *)exe; IMAGE_DOS_HEADER *dos; IMAGE_NT_HEADERS32 *nt;
    if (!exe || (uintptr_t)base!=HM_BODY_EXE_BASE ||
        !hm_body_readable(base,sizeof(IMAGE_DOS_HEADER))) return FALSE;
    dos=(IMAGE_DOS_HEADER *)base;
    if (dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>0x1000 ||
        !hm_body_readable(base+dos->e_lfanew,sizeof(IMAGE_NT_HEADERS32))) return FALSE;
    nt=(IMAGE_NT_HEADERS32 *)(base+dos->e_lfanew);
    return nt->Signature==IMAGE_NT_SIGNATURE &&
        nt->OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
        nt->FileHeader.Machine==IMAGE_FILE_MACHINE_I386 &&
        nt->OptionalHeader.ImageBase==HM_BODY_EXE_BASE &&
        nt->FileHeader.TimeDateStamp==HM_BODY_EXE_TIMESTAMP &&
        nt->OptionalHeader.SizeOfImage==HM_BODY_EXE_SIZE;
}

/* Install/uninstall only on the game thread while this site is not executing. */
static int hm_body_bridge_owned(void) {
    if (InterlockedCompareExchange(&hm_body_state,0,0)!=2) return 0;
    return hm_body_site_address &&
        hm_body_jump_owned(hm_body_site_address,(uintptr_t)hm_body_yaw_stub);
}
static int hm_body_bridge_install(HMODULE exe, HMNativeBodyCallback callback) {
    LONG state=InterlockedCompareExchange(&hm_body_state,0,0);
    if (state==3) return HM_BODY_UNCERTAIN;
    if (state==2) return callback==hm_body_callback && hm_body_bridge_owned()
                           ? HM_BODY_COMMITTED : HM_BODY_CLEAN_FAILURE;
    if (!callback || !hm_body_image_matches(exe)) return HM_BODY_CLEAN_FAILURE;
    BYTE *site=(BYTE *)exe+HM_BODY_SITE_RVA;
    if (!hm_body_readable(site,HM_BODY_STOLEN_BYTES+sizeof(hm_body_following_original)) ||
        memcmp(site,hm_body_site_original,HM_BODY_STOLEN_BYTES)!=0 ||
        memcmp(site+HM_BODY_STOLEN_BYTES,hm_body_following_original,
               sizeof(hm_body_following_original))!=0)
        return HM_BODY_CLEAN_FAILURE;
    hm_body_callback=callback;
    int result=hm_body_patch_install(site,(uintptr_t)hm_body_yaw_stub,
                                     hm_body_original_bytes,&hm_body_default_ops);
    if (result==HM_BODY_CLEAN_FAILURE) { hm_body_callback=NULL; return result; }
    hm_body_site_address=site;
    if (result==HM_BODY_UNCERTAIN) {
        hm_body_callback=NULL; /* Stub remains native-forward-only. */
        InterlockedExchange(&hm_body_state,3);
        return HM_BODY_UNCERTAIN;
    }
    InterlockedExchange(&hm_body_state,2);
    return HM_BODY_COMMITTED;
}
static int hm_body_bridge_uninstall(void) {
    LONG state=InterlockedCompareExchange(&hm_body_state,0,0);
    if (state==3) return HM_BODY_UNCERTAIN;
    if (state!=2 || !hm_body_site_address) return HM_BODY_CLEAN_FAILURE;
    int result=hm_body_patch_restore(hm_body_site_address,(uintptr_t)hm_body_yaw_stub,
                                     hm_body_original_bytes,&hm_body_default_ops);
    if (result==HM_BODY_CLEAN_FAILURE) return result; /* Still safely installed. */
    if (result==HM_BODY_UNCERTAIN) {
        hm_body_callback=NULL;
        InterlockedExchange(&hm_body_state,3);
        return result; /* Retain stub storage and native-forward-only behavior. */
    }
    hm_body_callback=NULL; hm_body_site_address=NULL;
    InterlockedExchange(&hm_body_state,0);
    return HM_BODY_COMMITTED;
}

#ifdef HOGSMOUSE_TEST
/* Test-only seam: inject callback and a continuation address, then invoke the
   exact naked trampoline against a synthetic pig vtable. Not in release builds. */
static void hm_body_test_set(HMNativeBodyCallback callback, DWORD continuation) {
    hm_body_callback=callback; hm_body_test_return_address=continuation;
}
static __declspec(naked) void __cdecl hm_body_test_invoke(BYTE *actor, BYTE *pig, int32_t yaw) {
    __asm {
        push ebp
        mov ebp, esp
        push ebx
        push esi
        push edi
        mov ebx, 0BEEFh
        lea eax, dword ptr [esp-8]
        mov dword ptr [hm_body_test_expected_method_esp], eax
        mov esi, dword ptr [ebp+8]
        mov ecx, dword ptr [ebp+0Ch]
        mov eax, dword ptr [ebp+10h]
        shl eax, 1
        xor edx, edx
        std
        jmp hm_body_yaw_stub
    }
}
static int hm_body_test_patch_install(BYTE *site, uintptr_t target,
                                      BYTE saved[HM_BODY_STOLEN_BYTES],
                                      const HMBodyPatchOps *ops) {
    return hm_body_patch_install(site,target,saved,ops);
}
static int hm_body_test_patch_restore(BYTE *site, uintptr_t target,
                                      const BYTE saved[HM_BODY_STOLEN_BYTES],
                                      const HMBodyPatchOps *ops) {
    return hm_body_patch_restore(site,target,saved,ops);
}
#endif
#endif
