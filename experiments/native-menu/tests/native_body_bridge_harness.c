#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define HOGSMOUSE_TEST 1
#include "../helpers/native_body_bridge.h"
#include "fixtures/body_test_policy.h"

volatile DWORD hmb_seen_calls, hmb_seen_return, hmb_seen_delta, hmb_seen_actor;
volatile DWORD hmb_seen_pig, hmb_seen_vtable, hmb_seen_esi, hmb_seen_ebx;
volatile DWORD hmb_seen_eax, hmb_seen_flags, hmb_seen_esp, hmb_callback_flags, hmb_callback_calls;
static BYTE hmb_actor[64];
static DWORD hmb_vtable[9];
static BYTE hmb_pig[32];
static HBBodyTokenRing hmb_tokens;
static uint32_t hmb_serial;

static DWORD hmb_read_flags(void) {
    __asm {
        pushfd
        pop eax
    }
}
static void __cdecl hmb_callback(BYTE *actor, BYTE *pig, int32_t *delta) {
    HBBodyCall c;
    hmb_callback_calls++;
    hmb_seen_actor=(DWORD)(uintptr_t)actor;
    hmb_seen_pig=(DWORD)(uintptr_t)pig;
    hmb_callback_flags=hmb_read_flags();
    memset(&c,0,sizeof(c));
    c.actor=(uintptr_t)actor; c.pig=(uintptr_t)pig;
    c.update_serial=hmb_serial; c.now_ms=100;
    c.human_type0=1; c.body_route=1; c.keyboard_lr_clear=1;
    c.network_manager_known=1; c.network_inactive=1;
    (void)hb_body_ring_apply(&hmb_tokens,&c,delta);
}

static __declspec(naked) void __cdecl hmb_native_method(void) {
    __asm {
        mov dword ptr [hmb_seen_eax], eax
        mov dword ptr [hmb_seen_esp], esp
        mov dword ptr [hmb_seen_esi], esi
        mov dword ptr [hmb_seen_ebx], ebx
        mov dword ptr [hmb_seen_vtable], edi
        mov dword ptr [hmb_seen_pig], ecx
        mov eax, dword ptr [esp]
        mov dword ptr [hmb_seen_return], eax
        mov eax, dword ptr [esp+4]
        mov dword ptr [hmb_seen_delta], eax
        pushfd
        pop eax
        mov dword ptr [hmb_seen_flags], eax
        inc dword ptr [hmb_seen_calls]
        cld
        ret 4
    }
}
static __declspec(naked) void __cdecl hmb_continue(void) {
    __asm {
        cld
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret
    }
}

static LONG WINAPI hmb_exception_filter(EXCEPTION_POINTERS *ep) {
    DWORD code=ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
    DWORD eip=ep && ep->ContextRecord ? ep->ContextRecord->Eip : 0;
    fprintf(stderr,"UNHANDLED EXCEPTION code=%08lX eip=%08lX\n",(unsigned long)code,(unsigned long)eip);
    fflush(stderr);
    ExitProcess(117);
    return EXCEPTION_EXECUTE_HANDLER;
}
static int fail(const char *s) { fprintf(stderr,"FAIL: %s\n",s); return 1; }
static int test_stub(void) {
    memset((void *)&hmb_seen_calls,0,sizeof(hmb_seen_calls));
    memset((void *)&hmb_callback_calls,0,sizeof(hmb_callback_calls));
    hmb_vtable[8]=(DWORD)(uintptr_t)hmb_native_method;
    *(DWORD *)hmb_pig=(DWORD)(uintptr_t)hmb_vtable;
    hmb_serial=7;
    memset(&hmb_tokens,0,sizeof(hmb_tokens));
    hb_body_ring_stage(&hmb_tokens,(uintptr_t)hmb_actor,(uintptr_t)hmb_pig,
                       hmb_serial,100,10,12,10,1,1,1,1,1);
    hm_body_test_set(hmb_callback,(DWORD)(uintptr_t)hmb_continue);
    hm_body_test_invoke(hmb_actor,hmb_pig,10);
    if (hmb_callback_calls!=1 || hmb_seen_calls!=1) return fail("callback/native method count");
    if (hmb_seen_return!=(DWORD)(uintptr_t)hmb_continue) return fail("original-style return path");
    if (hmb_seen_delta!=30) return fail("3x native yaw argument");
    if (hmb_seen_actor!=(DWORD)(uintptr_t)hmb_actor || hmb_seen_pig!=(DWORD)(uintptr_t)hmb_pig)
        return fail("actor/pig callback ABI");
    if (hmb_seen_esi!=(DWORD)(uintptr_t)hmb_actor || hmb_seen_vtable!=(DWORD)(uintptr_t)hmb_vtable ||
        hmb_seen_pig!=(DWORD)(uintptr_t)hmb_pig) return fail("saved registers / thiscall ABI");
    if (hmb_seen_ebx!=0xBEEF) return fail("EBX preserved");
    if (hmb_seen_eax!=10) return fail("wire calculation register preserved");
    if (hmb_seen_esp!=hm_body_test_expected_method_esp) return fail("ESP/ret4 argument layout");
    if ((hmb_seen_flags & 0xCD5u)!=0x404u) return fail("arithmetic flags preserved");
    if ((hmb_callback_flags & 0x400u) || !(hmb_seen_flags & 0x400u)) return fail("CLD callback / restored DF");
    return 0;
}

typedef struct { int protect_calls, flush_calls, fail_flush_a, fail_flush_b; } HMBFaults;
static HMBFaults hmb_faults;
static BOOL WINAPI hmb_mock_protect(LPVOID p,SIZE_T n,DWORD mode,PDWORD old) {
    (void)p;(void)n;(void)mode; hmb_faults.protect_calls++; *old=PAGE_EXECUTE_READ; return TRUE;
}
static BOOL WINAPI hmb_mock_flush(HANDLE p,LPCVOID a,SIZE_T n) {
    (void)p;(void)a;(void)n; hmb_faults.flush_calls++;
    return hmb_faults.flush_calls!=hmb_faults.fail_flush_a &&
           hmb_faults.flush_calls!=hmb_faults.fail_flush_b;
}
static int test_forward_only(void) {
    DWORD prior_calls=hmb_seen_calls;
    hm_body_test_set(NULL,(DWORD)(uintptr_t)hmb_continue);
    hm_body_test_invoke(hmb_actor,hmb_pig,7);
    if (hmb_seen_calls!=prior_calls+1 || hmb_seen_delta!=7)
        return fail("no-callback native-forward path");
    return 0;
}
static int test_transaction(void) {
    BYTE site[16]={0x8b,0x39,0x2b,0xc2,0xd1,0xf8,0x50}; BYTE saved[7]={0};
    HMBodyPatchOps ops={hmb_mock_protect,hmb_mock_flush};
    memset(&hmb_faults,0,sizeof(hmb_faults));
    uintptr_t target=(uintptr_t)site+0x80;
    if (hm_body_test_patch_install(site,target,saved,&ops)!=1 ||
        site[0]!=0xe9 || memcmp(saved,hm_body_site_original,7)) return fail("patch/install");
    if (hm_body_test_patch_restore(site,target,saved,&ops)!=1 ||
        memcmp(site,hm_body_site_original,7)) return fail("patch/restore");
    site[0]=0x90;
    if (hm_body_test_patch_install(site,target,saved,&ops)!=0)
        return fail("signature mismatch refusal");
    site[0]=0x8b; memset(&hmb_faults,0,sizeof(hmb_faults)); hmb_faults.fail_flush_a=1;
    if (hm_body_test_patch_install(site,target,saved,&ops)!=0 ||
        memcmp(site,hm_body_site_original,7)) return fail("clean rollback");
    memset(&hmb_faults,0,sizeof(hmb_faults)); hmb_faults.fail_flush_a=1; hmb_faults.fail_flush_b=2;
    if (hm_body_test_patch_install(site,target,saved,&ops)!=-1 ||
        memcmp(site,hm_body_site_original,7)) return fail("uncertain flush retains forward-only stub state");
    return 0;
}
int main(void) {
    setvbuf(stdout,NULL,_IONBF,0);
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    SetUnhandledExceptionFilter(hmb_exception_filter);
    puts("checkpoint: unsupported-image guard");
    if (hm_body_bridge_install((HMODULE)(uintptr_t)0x12340000,hmb_callback)!=0 ||
        hm_body_bridge_owned()!=0 || hm_body_bridge_uninstall()!=0)
        return fail("unsupported image / unowned uninstall refused");
    puts("checkpoint: native stub ABI");
    if (test_stub()) return 1;
    puts("checkpoint: native-forward-only");
    if (test_forward_only()) return 1;
    puts("checkpoint: patch transaction");
    if (test_transaction()) return 1;
    puts("PASS: native body bridge stub ABI, 32-bit arg replacement, return, DF/registers, patch transaction");
    return 0;
}
