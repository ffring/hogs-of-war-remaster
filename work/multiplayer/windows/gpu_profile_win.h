#ifndef HOGS_GPU_PROFILE_WIN_H
#define HOGS_GPU_PROFILE_WIN_H
#include <stddef.h>
typedef struct { const char *relative_path; const unsigned char *data; size_t size; const char *sha256; } HogsProfileFile;
int hogs_archive_guard_check(const char *game, const unsigned char *guard, size_t guard_len, char *message, size_t message_cap);
int hogs_gpu_profile_install(const char *game, const unsigned char *guard, size_t guard_len, const HogsProfileFile *files, size_t file_count, char *message, size_t message_cap);
int hogs_gpu_profile_restore(const char *game, const HogsProfileFile *files, size_t file_count, char *message, size_t message_cap);
#endif
