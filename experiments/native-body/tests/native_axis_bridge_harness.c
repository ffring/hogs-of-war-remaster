#define HOGSMOUSE_TEST 1
#include "native_axis_bridge.h"
#include <stdio.h>

#define CHECK(x, msg) do { if (!(x)) { \
    printf("FAIL: %s\n", msg); return 1; \
} } while (0)

static volatile DWORD g_callback_count;
static volatile DWORD g_seen_user;
static volatile DWORD g_seen_axes;
static volatile DWORD g_seen_mask_ptr;
static volatile DWORD g_expected_mask_ptr;
static volatile DWORD g_result_mask;
static volatile DWORD g_expected_esp;
static volatile DWORD g_actual_esp;
static volatile DWORD g_expected_ebx, g_actual_ebx;
static volatile DWORD g_expected_ecx, g_actual_ecx;
static volatile DWORD g_expected_edx, g_actual_edx;
static volatile DWORD g_expected_eax, g_actual_eax;
static volatile DWORD g_expected_ebp, g_actual_ebp;
static volatile DWORD g_expected_edi, g_actual_edi;
static volatile DWORD g_actual_esi;
static volatile DWORD g_expected_flags, g_actual_flags;
static volatile DWORD g_callback_df_clear;

/* Deliberately clobber all GPRs and flags; the bridge must restore the state
   that the game would have seen after its original MOV ESI,[ESP+134h]. */
static __declspec(naked) void __cdecl hm_test_callback(BYTE *user,
                                                       DWORD *axes,
                                                       DWORD *button_mask) {
    __asm {
        pushfd
        pop eax
        and eax, 400h
        shr eax, 10h
        xor eax, 1
        mov dword ptr [g_callback_df_clear], eax
        mov eax, dword ptr [esp+4]
        mov dword ptr [g_seen_user], eax
        mov ecx, dword ptr [esp+8]
        mov dword ptr [g_seen_axes], ecx
        mov edx, dword ptr [esp+0Ch]
        mov dword ptr [g_seen_mask_ptr], edx
        inc dword ptr [g_callback_count]
        mov dword ptr [ecx], 0A1B2C3D4h
        or dword ptr [edx], 00000020h
        mov eax, 11111111h
        mov ecx, 22222222h
        mov edx, 33333333h
        mov ebx, 44444444h
        mov esi, 55555555h
        mov edi, 66666666h
        mov ebp, 77777777h
        stc
        ret
    }
}

static __declspec(naked) void __cdecl hm_test_return(void) {
    __asm { ret }
}

/* Build the exact mid-function stack shape: at the hook entry, [ESP+0Ch] is
   the button word and [ESP+134h] is the axes pointer. CALL's return slot is
   accounted for when the fixture lays out its local memory. */
static __declspec(naked) void __cdecl hm_test_invoke(BYTE *user,
                                                      DWORD *axes,
                                                      DWORD initial_mask) {
    __asm {
        push ebp
        mov ebp, esp
        push ebx
        push esi
        push edi
        sub esp, 140h
        mov eax, dword ptr [ebp+0Ch]
        mov dword ptr [esp+130h], eax
        mov eax, dword ptr [ebp+10h]
        mov dword ptr [esp+8], eax
        lea eax, dword ptr [esp+8]
        mov dword ptr [g_expected_mask_ptr], eax
        mov edi, dword ptr [ebp+8]
        mov ebx, 11223344h
        mov ecx, 22334455h
        mov edx, 33445566h
        mov eax, 55667788h
        mov esi, 66778899h
        mov dword ptr [g_expected_esp], esp
        mov dword ptr [g_expected_ebx], ebx
        mov dword ptr [g_expected_ecx], ecx
        mov dword ptr [g_expected_edx], edx
        mov dword ptr [g_expected_eax], eax
        mov dword ptr [g_expected_ebp], ebp
        mov dword ptr [g_expected_edi], edi
        cmp eax, eax
        std
        pushfd
        pop dword ptr [g_expected_flags]
        call hm_native_axis_stub
        /* The stub jumps to hm_test_return, which RETs back to this point. */
        mov dword ptr [g_actual_eax], eax
        mov eax, dword ptr [esp+8]
        mov dword ptr [g_result_mask], eax
        mov dword ptr [g_actual_esp], esp
        mov dword ptr [g_actual_ebx], ebx
        mov dword ptr [g_actual_ecx], ecx
        mov dword ptr [g_actual_edx], edx
        mov dword ptr [g_actual_ebp], ebp
        mov dword ptr [g_actual_edi], edi
        mov dword ptr [g_actual_esi], esi
        pushfd
        pop dword ptr [g_actual_flags]
        cld
        lea esp, dword ptr [ebp-0Ch]
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret
    }
}

static int g_flush_calls;
static int g_fail_first_flush;
static int g_protect_calls;
static int g_fail_protect_call;
static BOOL WINAPI hm_test_flush(HANDLE process, LPCVOID address, SIZE_T size) {
    (void)process; (void)address; (void)size;
    g_flush_calls++;
    if (g_fail_first_flush && g_flush_calls == 1) return FALSE;
    return TRUE;
}

static BOOL WINAPI hm_test_protect(LPVOID address, SIZE_T size, DWORD mode,
                                   PDWORD old_mode) {
    g_protect_calls++;
    if (g_fail_protect_call && g_protect_calls == g_fail_protect_call)
        return FALSE;
    return VirtualProtect(address, size, mode, old_mode);
}

int main(void) {
    DWORD axes[4] = {0x12345678, 0, 0, 0};
    DWORD button_mask = 0x00000004;
    BYTE user[32] = {0};
    BYTE *site;
    BYTE saved[HM_NATIVE_STOLEN_BYTES] = {0};
    BYTE scratch[HM_NATIVE_STOLEN_BYTES];
    BYTE before[HM_NATIVE_STOLEN_BYTES];
    HMNativePatchOps ops = {hm_test_protect, hm_test_flush};
    BYTE jump[HM_NATIVE_STOLEN_BYTES];
    DWORD arithmetic_flags = 0x00000CD5u;

    CHECK(hm_native_identity_fields(HM_NATIVE_EXE_BASE, IMAGE_FILE_MACHINE_I386,
          HM_NATIVE_EXE_BASE, HM_NATIVE_EXE_TIMESTAMP, HM_NATIVE_EXE_SIZE),
          "supported PE identity accepted");
    CHECK(!hm_native_identity_fields(HM_NATIVE_EXE_BASE + 0x1000,
          IMAGE_FILE_MACHINE_I386, HM_NATIVE_EXE_BASE, HM_NATIVE_EXE_TIMESTAMP,
          HM_NATIVE_EXE_SIZE), "wrong loaded base refused");
    CHECK(!hm_native_identity_fields(HM_NATIVE_EXE_BASE, IMAGE_FILE_MACHINE_AMD64,
          HM_NATIVE_EXE_BASE, HM_NATIVE_EXE_TIMESTAMP, HM_NATIVE_EXE_SIZE),
          "wrong machine refused");
    CHECK(!hm_native_identity_fields(HM_NATIVE_EXE_BASE, IMAGE_FILE_MACHINE_I386,
          HM_NATIVE_EXE_BASE, HM_NATIVE_EXE_TIMESTAMP + 1, HM_NATIVE_EXE_SIZE),
          "wrong PE timestamp refused");
    CHECK(!hm_native_identity_fields(HM_NATIVE_EXE_BASE, IMAGE_FILE_MACHINE_I386,
          HM_NATIVE_EXE_BASE, HM_NATIVE_EXE_TIMESTAMP, HM_NATIVE_EXE_SIZE - 1),
          "wrong image size refused");
    CHECK(!hm_native_bridge_install(NULL, NULL),
          "production install refuses a missing module/callback");
    CHECK(!hm_native_bridge_uninstall(),
          "uninstall refuses when no production patch is owned");
    printf("PASS supported identity and unsupported PE refusals\n");

    hm_native_callback = hm_test_callback;
    hm_native_return_address = (BYTE *)hm_test_return;
    g_callback_count = 0;
    hm_test_invoke(user, axes, button_mask);
    CHECK(g_callback_count == 1, "callback called exactly once");
    CHECK(g_seen_user == (DWORD)(uintptr_t)user, "EDI user argument forwarded");
    CHECK(g_seen_axes == (DWORD)(uintptr_t)axes, "stolen MOV axes pointer forwarded");
    CHECK(g_seen_mask_ptr == g_expected_mask_ptr,
          "address of original ESP+0C button-mask slot forwarded");
    CHECK(axes[0] == 0xA1B2C3D4u, "callback can edit axes packet");
    CHECK(g_result_mask == 0x24, "callback can edit button-mask slot");
    CHECK(g_actual_esp == g_expected_esp, "original ESP restored");
    CHECK(g_actual_ebx == g_expected_ebx && g_actual_ecx == g_expected_ecx &&
          g_actual_edx == g_expected_edx && g_actual_eax == g_expected_eax,
          "volatile registers restored");
    CHECK(g_actual_ebp == g_expected_ebp && g_actual_edi == g_expected_edi,
          "nonvolatile registers restored");
    CHECK(g_actual_esi == (DWORD)(uintptr_t)axes,
          "ESI preserves the result of the stolen MOV");
    CHECK(((g_actual_flags ^ g_expected_flags) & arithmetic_flags) == 0,
          "arithmetic EFLAGS restored");
    CHECK((g_actual_flags & 0x400u) != 0, "original DF restored at return");
    CHECK(g_callback_df_clear == 1, "DF cleared while entering C callback");
    printf("PASS callback ABI: args, edits, ESP/GPR/EFLAGS, callback DF\n");

    site = (BYTE *)VirtualAlloc(NULL, 4096, MEM_RESERVE | MEM_COMMIT,
                                PAGE_EXECUTE_READWRITE);
    CHECK(site != NULL, "allocate mock executable patch site");
    memcpy(site, hm_native_site_original, HM_NATIVE_STOLEN_BYTES);
    memcpy(before, site, sizeof(before));
    CHECK(hm_native_make_jump(site, (uintptr_t)(site + 128), jump),
          "build relative JMP");
    CHECK(hm_native_test_patch_install(site, (uintptr_t)(site + 128), saved,
          &hm_native_default_ops), "install mock JMP patch");
    CHECK(memcmp(site, jump, sizeof(jump)) == 0 &&
          hm_native_test_jump_owned(site, (uintptr_t)(site + 128)),
          "verify E9 rel32 plus two NOPs");
    CHECK(!hm_native_test_patch_restore(site, (uintptr_t)(site + 96), saved,
          &hm_native_default_ops), "refuse rollback when JMP target is foreign");
    CHECK(hm_native_test_patch_restore(site, (uintptr_t)(site + 128), saved,
          &hm_native_default_ops), "rollback owned JMP");
    CHECK(memcmp(site, before, sizeof(before)) == 0,
          "rollback restores all seven original bytes");
    printf("PASS mock-site install, ownership guard, and exact rollback\n");

    site[3] ^= 1;
    memcpy(before, site, sizeof(before));
    CHECK(!hm_native_test_patch_install(site, (uintptr_t)(site + 128), scratch,
          &hm_native_default_ops), "refuse mixed/unsupported original bytes");
    CHECK(memcmp(site, before, sizeof(before)) == 0,
          "signature refusal does not mutate mock site");
    site[3] ^= 1;
    printf("PASS mixed-byte refusal without mutation\n");

    memcpy(site, hm_native_site_original, HM_NATIVE_STOLEN_BYTES);
    g_flush_calls = 0; g_fail_first_flush = 1;
    ops.protect = hm_test_protect; ops.flush = hm_test_flush;
    CHECK(!hm_native_test_patch_install(site, (uintptr_t)(site + 128), scratch,
          &ops), "report a cache-flush failure");
    CHECK(memcmp(site, hm_native_site_original, HM_NATIVE_STOLEN_BYTES) == 0,
          "failed install rolls bytes back");
    g_fail_first_flush = 0;
    printf("PASS injected post-write failure restores original bytes\n");

    g_protect_calls = 0; g_fail_protect_call = 2;
    CHECK(!hm_native_test_patch_install(site, (uintptr_t)(site + 128), scratch,
          &ops), "report a failed page-protection restore");
    CHECK(memcmp(site, hm_native_site_original, HM_NATIVE_STOLEN_BYTES) == 0,
          "failed protection restore still rolls the bytes back");
    g_fail_protect_call = 0;
    printf("PASS failed page-protection restore rolls back bytes\n");

    VirtualFree(site, 0, MEM_RELEASE);
    hm_native_callback = NULL;
    hm_native_return_address = NULL;
    puts("All native-axis bridge harness checks passed.");
    return 0;
}
