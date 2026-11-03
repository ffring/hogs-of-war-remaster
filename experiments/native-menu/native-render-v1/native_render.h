#ifndef HOGS_NATIVE_RENDER_H
#define HOGS_NATIVE_RENDER_H
#include "../native-menu-policy-v1/menu_policy.h"
/* Called exclusively on the original UI thread, inside the native sprite batch. */
int hmr_ready(void);
int hmr_draw_menu(const HMMenuState *menu, int error);
int hmr_draw_options(int selected);
#endif
