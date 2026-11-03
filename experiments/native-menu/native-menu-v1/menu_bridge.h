#ifndef HOGS_MENU_BRIDGE_H
#define HOGS_MENU_BRIDGE_H

#include <stdint.h>

#ifdef _WIN32
#  if defined(_MSC_VER)
#    define HMN_CDECL __cdecl
#    define HMN_FASTCALL __fastcall
#  else
#    define HMN_CDECL __attribute__((cdecl))
#    define HMN_FASTCALL __attribute__((fastcall))
#  endif
#else
#  define HMN_CDECL
#  define HMN_FASTCALL
#endif

enum HmnAction {
    HMN_ACTION_CONFIRM = 1,
    HMN_ACTION_BACK = 2,
    HMN_ACTION_UP = 3,
    HMN_ACTION_DOWN = 4,
    HMN_ACTION_LEFT = 5,
    HMN_ACTION_RIGHT = 6
};

/* Implemented by the menu/UI integration owner. Return nonzero to consume. */
int HMN_CDECL hmn_action(int action);
void HMN_CDECL hmn_after_frame(void);
void HMN_CDECL hmn_before_present(void);
void HMN_CDECL hmn_before_sprite_batch(void);
void HMN_CDECL hmn_wait_enter(void);
void HMN_CDECL hmn_wait_leave(void);
int HMN_CDECL hmn_wait_input(int native_input);
const char *HMN_CDECL hmn_text_override(void *font, int text_id);

/* Call on the game's UI thread before its frontend message loop starts. */
int HMN_CDECL hmn_install(void);
int HMN_CDECL hmn_uninstall(void);

#ifdef HOGSMENU_TEST
typedef void (HMN_CDECL *HmnConfirmOriginal)(void *arg);
typedef void (HMN_CDECL *HmnActionOriginal)(void);
typedef int (HMN_CDECL *HmnFrameOriginal)(int first, int second);
typedef void (HMN_CDECL *HmnPresentOriginal)(void);
typedef uintptr_t (HMN_CDECL *HmnSpriteBatchOriginal)(void);
typedef int (HMN_CDECL *HmnWaitInputOriginal)(void);
typedef uintptr_t (HMN_CDECL *HmnWaitCycleOriginal)(void);
typedef const char *(HMN_FASTCALL *HmnLookupOriginal)(void *font, void *edx, int text_id);

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
                             HmnLookupOriginal lookup);
void hmn_test_set_ui_enabled(int enabled);
void HMN_CDECL hmn_test_confirm_wrapper(void *arg);
void HMN_CDECL hmn_test_back_wrapper(void);
void HMN_CDECL hmn_test_up_wrapper(void);
void HMN_CDECL hmn_test_down_wrapper(void);
void HMN_CDECL hmn_test_left_wrapper(void);
void HMN_CDECL hmn_test_right_wrapper(void);
int HMN_CDECL hmn_test_frame_wrapper(int first, int second);
void HMN_CDECL hmn_test_present_wrapper(void);
uintptr_t HMN_CDECL hmn_test_sprite_batch_wrapper(void);
uintptr_t HMN_CDECL hmn_test_wait_cycle_wrapper(void);
int HMN_CDECL hmn_test_wait_input_wrapper(void);
const char *HMN_FASTCALL hmn_test_lookup_wrapper(void *font, void *edx, int text_id);
#endif

#endif
