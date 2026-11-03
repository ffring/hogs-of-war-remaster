#ifndef HOGS_NATIVE_RUNTIME_H
#define HOGS_NATIVE_RUNTIME_H

#include <stdint.h>
#include "../native-menu-policy-v1/menu_policy.h"

#ifdef _WIN32
#  if defined(_MSC_VER)
#    define HNR_CDECL __cdecl
#  else
#    define HNR_CDECL __attribute__((cdecl))
#  endif
#else
#  define HNR_CDECL
#endif

typedef void (HNR_CDECL *HnrSetWaitFn)(int active);
typedef void (HNR_CDECL *HnrClearInputFn)(void);

/* Call from the game thread after the settings path and host callbacks exist.
   The supplied path must be an absolute path to this installation's INI. */
int HNR_CDECL hmn_runtime_init(const char *ini_path,
                               HnrSetWaitFn set_wait_active,
                               HnrClearInputFn clear_input);
/* Returns zero if owned row pointers cannot be restored safely; keep module loaded. */
int HNR_CDECL hmn_runtime_shutdown(void);
int HNR_CDECL hmn_runtime_ui_active(void);
const HMMenuState *HNR_CDECL hmn_runtime_menu_state(void);

/* Bridge callbacks. These names are linked by menu_bridge.c. */
int HNR_CDECL hmn_action(int action);
void HNR_CDECL hmn_after_frame(void);
void HNR_CDECL hmn_before_present(void);
void HNR_CDECL hmn_before_sprite_batch(void);
void HNR_CDECL hmn_wait_enter(void);
void HNR_CDECL hmn_wait_leave(void);
int HNR_CDECL hmn_wait_input(int native_input);
const char *HNR_CDECL hmn_text_override(void *font, int text_id);

#endif
