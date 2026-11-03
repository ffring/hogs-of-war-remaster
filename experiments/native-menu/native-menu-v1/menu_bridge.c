#include "menu_bridge.h"
#include "menu_bridge_core.h"

#include <string.h>

typedef void (HMN_CDECL *HmnConfirmFn)(void *arg);
typedef void (HMN_CDECL *HmnActionFn)(void);
typedef int (HMN_CDECL *HmnFrameFn)(int first, int second);
typedef void (HMN_CDECL *HmnPresentFn)(void);
typedef uintptr_t (HMN_CDECL *HmnSpriteBatchFn)(void);
typedef uintptr_t (HMN_CDECL *HmnWaitCycleFn)(void);
typedef int (HMN_CDECL *HmnWaitInputFn)(void);
typedef const char *(HMN_FASTCALL *HmnTextFn)(void *font, void *edx, int text_id);

static HmnConfirmFn g_original_confirm;
static HmnActionFn g_original_back;
static HmnActionFn g_original_up;
static HmnActionFn g_original_down;
static HmnActionFn g_original_left;
static HmnActionFn g_original_right;
static HmnFrameFn g_original_frame;
static HmnPresentFn g_original_present;
static HmnSpriteBatchFn g_original_sprite_batch;
static HmnWaitCycleFn g_original_wait_cycle;
static HmnWaitInputFn g_original_wait_input;
static HmnTextFn g_original_text;
static int hmn_hook_use_ui(void);

static void HMN_CDECL hmn_confirm_hook(void *arg) {
    if (hmn_hook_use_ui() && hmn_action(HMN_ACTION_CONFIRM)) return;
    if (g_original_confirm) g_original_confirm(arg);
}
static void HMN_CDECL hmn_back_hook(void) {
    if ((!hmn_hook_use_ui() || !hmn_action(HMN_ACTION_BACK)) && g_original_back) g_original_back();
}
static void HMN_CDECL hmn_up_hook(void) {
    if ((!hmn_hook_use_ui() || !hmn_action(HMN_ACTION_UP)) && g_original_up) g_original_up();
}
static void HMN_CDECL hmn_down_hook(void) {
    if ((!hmn_hook_use_ui() || !hmn_action(HMN_ACTION_DOWN)) && g_original_down) g_original_down();
}
static void HMN_CDECL hmn_left_hook(void) {
    if ((!hmn_hook_use_ui() || !hmn_action(HMN_ACTION_LEFT)) && g_original_left) g_original_left();
}
static void HMN_CDECL hmn_right_hook(void) {
    if ((!hmn_hook_use_ui() || !hmn_action(HMN_ACTION_RIGHT)) && g_original_right) g_original_right();
}
static int HMN_CDECL hmn_frame_hook(int first, int second) {
    int result = g_original_frame ? g_original_frame(first, second) : 0;
    if (hmn_hook_use_ui()) hmn_after_frame();
    return result;
}
static void HMN_CDECL hmn_present_hook(void) {
    if (hmn_hook_use_ui()) hmn_before_present();
    if (g_original_present) g_original_present();
}
static uintptr_t HMN_CDECL hmn_sprite_batch_hook(void) {
    uintptr_t result = g_original_sprite_batch ? g_original_sprite_batch() : 0;
    if (hmn_hook_use_ui()) hmn_before_sprite_batch();
    return result;
}
static uintptr_t HMN_CDECL hmn_wait_cycle_hook(void) {
    uintptr_t result = 0;
    int use_ui = hmn_hook_use_ui();
    if (use_ui) hmn_wait_enter();
    if (g_original_wait_cycle) result = g_original_wait_cycle();
    if (use_ui) hmn_wait_leave();
    return result;
}
static int HMN_CDECL hmn_wait_input_hook(void) {
    int native_input = g_original_wait_input ? g_original_wait_input() : 0;
    if (!hmn_hook_use_ui()) return native_input;
    /* The retail caller tests AL, not the full EAX register. Preserve that
       boolean contract even if a future/alternate callee leaves high bits set. */
    return hmn_wait_input(((uint8_t)native_input) != 0);
}
static const char *HMN_FASTCALL hmn_text_hook(void *font, void *edx, int text_id) {
    if (hmn_hook_use_ui()) {
        const char *replacement = hmn_text_override(font, text_id);
        if (replacement) return replacement;
    }
    return g_original_text ? g_original_text(font, edx, text_id) : NULL;
}

#ifdef HOGSMENU_TEST

static int g_test_bridge_state = 2;
static int hmn_hook_use_ui(void) { return g_test_bridge_state == 2; }
void hmn_test_set_ui_enabled(int enabled) { g_test_bridge_state = enabled ? 2 : 3; }

void hmn_test_bind_originals(HmnConfirmOriginal confirm,
                             HmnActionOriginal back,
                             HmnActionOriginal up,
                             HmnActionOriginal down,
                             HmnActionOriginal left,
                             HmnActionOriginal right,
                             HmnFrameOriginal frame,
                             HmnPresentOriginal present,
                             HmnSpriteBatchOriginal sprite_batch,
                             HmnWaitCycleOriginal wait_cycle,
                             HmnWaitInputOriginal wait_input,
                             HmnLookupOriginal lookup) {
    g_original_confirm = confirm; g_original_back = back;
    g_original_up = up; g_original_down = down; g_original_left = left;
    g_original_right = right; g_original_frame = frame;
    g_original_present = present; g_original_sprite_batch = sprite_batch;
    g_original_wait_cycle = wait_cycle;
    g_original_wait_input = wait_input;
    g_original_text = lookup;
}
void HMN_CDECL hmn_test_confirm_wrapper(void *arg) { hmn_confirm_hook(arg); }
void HMN_CDECL hmn_test_back_wrapper(void) { hmn_back_hook(); }
void HMN_CDECL hmn_test_up_wrapper(void) { hmn_up_hook(); }
void HMN_CDECL hmn_test_down_wrapper(void) { hmn_down_hook(); }
void HMN_CDECL hmn_test_left_wrapper(void) { hmn_left_hook(); }
void HMN_CDECL hmn_test_right_wrapper(void) { hmn_right_hook(); }
int HMN_CDECL hmn_test_frame_wrapper(int first, int second) {
    return hmn_frame_hook(first, second);
}
void HMN_CDECL hmn_test_present_wrapper(void) { hmn_present_hook(); }
uintptr_t HMN_CDECL hmn_test_sprite_batch_wrapper(void) {
    return hmn_sprite_batch_hook();
}
uintptr_t HMN_CDECL hmn_test_wait_cycle_wrapper(void) {
    return hmn_wait_cycle_hook();
}
int HMN_CDECL hmn_test_wait_input_wrapper(void) { return hmn_wait_input_hook(); }
const char *HMN_FASTCALL hmn_test_lookup_wrapper(void *font, void *edx, int text_id) {
    return hmn_text_hook(font, edx, text_id);
}

#else

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct HmnLivePatch {
    uint8_t *address;
    SIZE_T length;
    DWORD old_protection;
} HmnLivePatch;

static HmnPatchPlan g_plan;
static uint8_t *g_image;
static uint8_t *g_text_trampoline;
static volatile LONG g_state; /* 0 uninstalled, 1 installing, 2 installed, 3 degraded/uncertain */
static int hmn_hook_use_ui(void) { return InterlockedCompareExchange(&g_state, 0, 0) == 2; }

static int hmn_live_protect(void *context, uint8_t *address, size_t length,
                            int writable, uint32_t *old_protection) {
    DWORD previous = 0;
    DWORD desired = writable ? PAGE_EXECUTE_READWRITE : *old_protection;
    (void)context;
    if (!VirtualProtect(address, (SIZE_T)length, desired, &previous)) return 0;
    if (writable) *old_protection = previous;
    return 1;
}

static int hmn_live_flush(void *context, const uint8_t *address, size_t length) {
    (void)context;
    return FlushInstructionCache(GetCurrentProcess(), address, (SIZE_T)length) != 0;
}

static int hmn_live_identity(uint8_t *base, HmnIdentity *id) {
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)base;
    const IMAGE_NT_HEADERS32 *nt;
    if (!base || (uintptr_t)base != HMN_IMAGE_BASE || dos->e_magic != IMAGE_DOS_SIGNATURE ||
        dos->e_lfanew < (LONG)sizeof(IMAGE_DOS_HEADER) ||
        (uint32_t)dos->e_lfanew > HMN_IMAGE_SIZE - sizeof(IMAGE_NT_HEADERS32)) return 0;
    nt = (const IMAGE_NT_HEADERS32 *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        return 0;
    id->loaded_base = (uint32_t)(uintptr_t)base;
    id->preferred_base = nt->OptionalHeader.ImageBase;
    id->image_size = nt->OptionalHeader.SizeOfImage;
    id->timestamp = nt->FileHeader.TimeDateStamp;
    id->machine = nt->FileHeader.Machine;
    return hmn_identity_supported(id);
}

static int hmn_current_hooks(uint8_t *base, const uint32_t hooks[HMN_PATCH_COUNT],
                             HmnPatchPlan *plan) {
    HmnIdentity id;
    return hmn_live_identity(base, &id) &&
           hmn_make_plan(&id, base, hooks, plan);
}

static int hmn_apply_live(const HmnPatchPlan *plan) {
    HmnMemoryOps ops;
    ops.context = NULL; ops.protect = hmn_live_protect; ops.flush = hmn_live_flush;
    return hmn_apply_plan(plan, g_image, &ops);
}

int HMN_CDECL hmn_install(void) {
    uint8_t *base;
    uint32_t hooks[HMN_PATCH_COUNT];
    if (InterlockedCompareExchange(&g_state, 1, 0) != 0) return g_state == 2;
    base = (uint8_t *)GetModuleHandleW(NULL);
    hooks[HMN_HOOK_CONFIRM] = (uint32_t)(uintptr_t)hmn_confirm_hook;
    hooks[HMN_HOOK_BACK] = (uint32_t)(uintptr_t)hmn_back_hook;
    hooks[HMN_HOOK_UP] = (uint32_t)(uintptr_t)hmn_up_hook;
    hooks[HMN_HOOK_DOWN] = (uint32_t)(uintptr_t)hmn_down_hook;
    hooks[HMN_HOOK_LEFT] = (uint32_t)(uintptr_t)hmn_left_hook;
    hooks[HMN_HOOK_RIGHT] = (uint32_t)(uintptr_t)hmn_right_hook;
    hooks[HMN_HOOK_AFTER_FRAME] = (uint32_t)(uintptr_t)hmn_frame_hook;
    hooks[HMN_HOOK_BEFORE_PRESENT] = (uint32_t)(uintptr_t)hmn_present_hook;
    hooks[HMN_HOOK_SPRITE_BATCH] = (uint32_t)(uintptr_t)hmn_sprite_batch_hook;
    hooks[HMN_HOOK_WAIT_CYCLE] = (uint32_t)(uintptr_t)hmn_wait_cycle_hook;
    hooks[HMN_HOOK_WAIT_INPUT] = (uint32_t)(uintptr_t)hmn_wait_input_hook;

    g_image = base;
    if (!hmn_live_identity(base, &(HmnIdentity){0})) goto fail;
    g_text_trampoline = (uint8_t *)VirtualAlloc(NULL, 12, MEM_COMMIT | MEM_RESERVE,
                                               PAGE_EXECUTE_READWRITE);
    if (!g_text_trampoline) goto fail;
    /* Text hook trampoline preserves both complete original instructions. */
    memcpy(g_text_trampoline, base + hmn_text_rva, HMN_TEXT_BYTES);
    g_text_trampoline[7] = 0xE9;
    {
        int32_t back = (int32_t)((base + hmn_text_rva + HMN_TEXT_BYTES) -
                                 (g_text_trampoline + 12));
        memcpy(g_text_trampoline + 8, &back, sizeof(back));
    }
    if (!FlushInstructionCache(GetCurrentProcess(), g_text_trampoline, 12)) goto fail;
    hooks[HMN_HOOK_TEXT] = (uint32_t)(uintptr_t)hmn_text_hook;

    if (!hmn_current_hooks(base, hooks, &g_plan)) goto fail;
    /* Derive forwarding targets from the exact checked CALL instructions. */
    g_original_confirm = (HmnConfirmFn)(uintptr_t)hmn_rel_target(HMN_IMAGE_BASE, g_plan.patches[0].rva, g_plan.patches[0].original);
    g_original_back = (HmnActionFn)(uintptr_t)hmn_rel_target(HMN_IMAGE_BASE, g_plan.patches[1].rva, g_plan.patches[1].original);
    g_original_up = (HmnActionFn)(uintptr_t)hmn_rel_target(HMN_IMAGE_BASE, g_plan.patches[2].rva, g_plan.patches[2].original);
    g_original_down = (HmnActionFn)(uintptr_t)hmn_rel_target(HMN_IMAGE_BASE, g_plan.patches[3].rva, g_plan.patches[3].original);
    g_original_left = (HmnActionFn)(uintptr_t)hmn_rel_target(HMN_IMAGE_BASE, g_plan.patches[4].rva, g_plan.patches[4].original);
    g_original_right = (HmnActionFn)(uintptr_t)hmn_rel_target(HMN_IMAGE_BASE, g_plan.patches[5].rva, g_plan.patches[5].original);
    g_original_frame = (HmnFrameFn)(uintptr_t)hmn_rel_target(HMN_IMAGE_BASE, g_plan.patches[6].rva, g_plan.patches[6].original);
    g_original_present = (HmnPresentFn)(uintptr_t)hmn_rel_target(HMN_IMAGE_BASE, g_plan.patches[7].rva, g_plan.patches[7].original);
    g_original_sprite_batch = (HmnSpriteBatchFn)(uintptr_t)hmn_rel_target(HMN_IMAGE_BASE, g_plan.patches[8].rva, g_plan.patches[8].original);
    g_original_wait_cycle = (HmnWaitCycleFn)(uintptr_t)hmn_rel_target(HMN_IMAGE_BASE, g_plan.patches[9].rva, g_plan.patches[9].original);
    g_original_wait_input = (HmnWaitInputFn)(uintptr_t)hmn_rel_target(HMN_IMAGE_BASE, g_plan.patches[10].rva, g_plan.patches[10].original);
    g_original_text = (HmnTextFn)(uintptr_t)g_text_trampoline;
    {
        int applied = hmn_apply_live(&g_plan);
        if (applied < 0) {
            /* Some CPU caches may still hold calls into these hooks. Keep every
               forwarding target and trampoline resident, but turn off custom UI. */
            InterlockedExchange(&g_state, 3);
            return -1;
        }
        if (!applied) goto fail;
    }
    InterlockedExchange(&g_state, 2);
    return 1;

fail:
    if (g_text_trampoline) VirtualFree(g_text_trampoline, 0, MEM_RELEASE);
    g_text_trampoline = NULL; g_image = NULL;
    g_original_confirm = NULL; g_original_back = g_original_up = NULL;
    g_original_down = g_original_left = g_original_right = NULL;
    g_original_frame = NULL; g_original_present = NULL; g_original_text = NULL;
    g_original_sprite_batch = NULL;
    g_original_wait_cycle = NULL; g_original_wait_input = NULL;
    InterlockedExchange(&g_state, 0);
    return 0;
}

int HMN_CDECL hmn_uninstall(void) {
    HmnPatchPlan reverse;
    if (InterlockedCompareExchange(&g_state, 1, 2) != 2) return 0;
    if (!g_image || !hmn_plan_matches(&g_plan, g_image, 1)) {
        InterlockedExchange(&g_state, 2); return 0;
    }
    hmn_reverse_plan(&g_plan, &reverse);
    {
        int applied = hmn_apply_live(&reverse);
        if (applied < 0) { InterlockedExchange(&g_state, 3); return 0; }
        if (!applied) { InterlockedExchange(&g_state, 2); return 0; }
    }
    if (g_text_trampoline) VirtualFree(g_text_trampoline, 0, MEM_RELEASE);
    g_text_trampoline = NULL; g_image = NULL;
    InterlockedExchange(&g_state, 0);
    return 1;
}

#endif
