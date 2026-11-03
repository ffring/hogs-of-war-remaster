#include "native_ini.h"
HogsIniResult hogs_mouse_settings_load_path(const char *path, HogsMouseSettings *out) { return hogs_ini_load(path, out); }
HogsIniResult hogs_mouse_settings_save_path(const char *path, const HogsMouseSettings *in) { return hogs_ini_save(path, in); }
