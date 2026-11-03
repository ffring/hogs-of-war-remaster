#ifndef HOGS_NATIVE_AIM_BRIDGE_H
#define HOGS_NATIVE_AIM_BRIDGE_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "native_aim_patch_policy.h"

#if !defined(_M_IX86) && !defined(__i386__)
#error Native aim detour is supported only for x86 Windows.
#endif

typedef int (__thiscall *HMNativeAimOriginal)(void *actor);
static HMNativeAimOriginal volatile hmna_original_handler;
static volatile LONG hmna_state;
static BYTE *volatile hmna_site;
static BYTE *volatile hmna_trampoline_memory;

static LONG hmna_current_state(void) {
    return InterlockedCompareExchange(&hmna_state,0,0);
}

static int hmna_can_forward(void) {
    return hmna_original_handler != NULL;
}

static int hmna_supported_image(HMODULE module) {
    BYTE *base=(BYTE *)module;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS32 *nt;
    if(!base || (uintptr_t)base!=0x00400000u) return 0;
    dos=(IMAGE_DOS_HEADER *)base;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>0x1000) return 0;
    nt=(IMAGE_NT_HEADERS32 *)(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC) return 0;
    return hmna_identity((uintptr_t)base,nt->FileHeader.Machine,nt->OptionalHeader.ImageBase,
                         nt->FileHeader.TimeDateStamp,nt->OptionalHeader.SizeOfImage);
}

/* Call on the game's thread before entering the target input handler. */
static int hmna_install(HMODULE module, void *detour) {
    BYTE *site;
    BYTE patch[HMNA_STOLEN];
    BYTE original[HMNA_STOLEN];
    DWORD old_protection=0, ignored=0;
    BYTE *trampoline;
    LONG previous=InterlockedCompareExchange(&hmna_state,0,0);
    if(previous==2) return 1;
    if(previous==3) return -1;
    if(InterlockedCompareExchange(&hmna_state,1,0)!=0) return -1;
    if(!detour || !hmna_supported_image(module)) { InterlockedExchange(&hmna_state,0); return 0; }
    site=(BYTE *)module+(HMNA_SITE_VA-0x00400000u);
    if(!hmna_original_matches(site) || !hmna_jump((uintptr_t)site,(uintptr_t)detour,patch)) {
        InterlockedExchange(&hmna_state,0); return 0;
    }
    trampoline=(BYTE *)VirtualAlloc(NULL,HMNA_STOLEN+5,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    if(!trampoline || !hmna_trampoline((uintptr_t)site,(uintptr_t)trampoline,trampoline)) {
        if(trampoline) VirtualFree(trampoline,0,MEM_RELEASE);
        InterlockedExchange(&hmna_state,0);
        return 0;
    }
    if(!FlushInstructionCache(GetCurrentProcess(),trampoline,HMNA_STOLEN+5)) {
        VirtualFree(trampoline,0,MEM_RELEASE);
        InterlockedExchange(&hmna_state,0);
        return 0;
    }
    memcpy(original,site,HMNA_STOLEN);
    if(!VirtualProtect(site,HMNA_STOLEN,PAGE_EXECUTE_READWRITE,&old_protection)) {
        VirtualFree(trampoline,0,MEM_RELEASE); InterlockedExchange(&hmna_state,0); return 0;
    }
    /* Refuse a changed target between validation and patch. */
    if(memcmp(site,original,HMNA_STOLEN)!=0 || !hmna_original_matches(site)) {
        (void)VirtualProtect(site,HMNA_STOLEN,old_protection,&ignored);
        VirtualFree(trampoline,0,MEM_RELEASE);
        InterlockedExchange(&hmna_state,0);
        return 0;
    }
    hmna_original_handler=(HMNativeAimOriginal)trampoline;
    hmna_site=site; hmna_trampoline_memory=trampoline;
    memcpy(site,patch,HMNA_STOLEN);
    if(!FlushInstructionCache(GetCurrentProcess(),site,HMNA_STOLEN)) {
        memcpy(site,original,HMNA_STOLEN);
        BOOL rollback_flushed=FlushInstructionCache(GetCurrentProcess(),site,HMNA_STOLEN);
        BOOL protection_restored=VirtualProtect(site,HMNA_STOLEN,old_protection,&ignored);
        if(rollback_flushed && protection_restored) {
            hmna_original_handler=NULL;
            hmna_site=NULL;
            hmna_trampoline_memory=NULL;
            VirtualFree(trampoline,0,MEM_RELEASE);
            InterlockedExchange(&hmna_state,0);
            return 0;
        }
        hmna_original_handler=(HMNativeAimOriginal)trampoline;
        hmna_site=site; hmna_trampoline_memory=trampoline;
        InterlockedExchange(&hmna_state,3);
        return -1;
    }
    if(!VirtualProtect(site,HMNA_STOLEN,old_protection,&ignored)) {
        /* The hook bytes are complete and flushed. Keep the live trampoline
           and detour stable; the target page remains RWX until process exit. */
        hmna_original_handler=(HMNativeAimOriginal)trampoline;
        hmna_site=site; hmna_trampoline_memory=trampoline;
        InterlockedExchange(&hmna_state,2);
        return 1;
    }
    InterlockedExchange(&hmna_state,2);
    return 1;
}

static int hmna_owned(void) {
    return hmna_current_state()==2 && hmna_site &&
           hmna_trampoline_memory && hmna_original_handler;
}

#endif
