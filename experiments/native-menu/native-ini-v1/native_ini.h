#ifndef HOGS_NATIVE_INI_H
#define HOGS_NATIVE_INI_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HogsMouseSettings {
    int native_units_per_pixel;
    int aim_sensitivity_percent;
    int invert_x;
    int invert_y;
} HogsMouseSettings;

typedef enum HogsIniResult {
    HOGS_INI_OK = 0,
    HOGS_INI_BAD_ARGUMENT,
    HOGS_INI_PATH_ERROR,
    HOGS_INI_REPARSE_REFUSED,
    HOGS_INI_DIRECTORY_REFUSED,
    HOGS_INI_TOO_LARGE,
    HOGS_INI_MALFORMED,
    HOGS_INI_IO_ERROR,
    HOGS_INI_PROFILE_WRITE_FAILED,
    HOGS_INI_READBACK_FAILED,
    HOGS_INI_COMMIT_FAILED,
    HOGS_INI_ROLLBACK_FAILED
} HogsIniResult;

#ifdef HOGSMOUSE_TEST
#define HOGS_INI_TEST_FAIL_PRECOMMIT 1u
#define HOGS_INI_TEST_FAIL_POSTCOMMIT 2u
#define HOGS_INI_TEST_MUTATE_BEFORE_COMMIT 4u
typedef struct HogsIniOps {
    unsigned test_failures;
} HogsIniOps;
#endif

static __inline HogsMouseSettings hogs_ini_defaults(void) {
    HogsMouseSettings s = { 12, 300, 1, 1 };
    return s;
}

static __inline int hogs_ini_validate(const HogsMouseSettings *s) {
    return s && s->native_units_per_pixel >= 1 && s->native_units_per_pixel <= 24 &&
           s->aim_sensitivity_percent >= 25 && s->aim_sensitivity_percent <= 1000 &&
           (s->invert_x == 0 || s->invert_x == 1) && (s->invert_y == 0 || s->invert_y == 1);
}

static __inline int hogs_ini_parse_int(const char *s, int *out) {
    char *end = NULL;
    long v;
    if (!s || !*s || !out) return 0;
    v = strtol(s, &end, 10);
    if (end == s || !end || *end != '\0' || v < -2147483647L - 1L || v > 2147483647L) return 0;
    *out = (int)v;
    return 1;
}

static __inline HogsIniResult hogs_ini_check_path(const char *path, DWORD *out_size, int *exists) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    char parent[MAX_PATH];
    char *slash;
    DWORD attrs;
    if (!path || !*path || strlen(path) >= MAX_PATH || !out_size || !exists) return HOGS_INI_BAD_ARGUMENT;
    if (!((path[0] && path[1] == ':' && (path[2] == '\\' || path[2] == '/')) ||
          (path[0] == '\\' && path[1] == '\\') || (path[0] == '/' && path[1] == '/'))) return HOGS_INI_BAD_ARGUMENT;
    attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        DWORD e = GetLastError();
        if (e != ERROR_FILE_NOT_FOUND && e != ERROR_PATH_NOT_FOUND) return HOGS_INI_PATH_ERROR;
        *exists = 0;
        *out_size = 0;
    } else {
        if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) return HOGS_INI_REPARSE_REFUSED;
        if (attrs & FILE_ATTRIBUTE_DIRECTORY) return HOGS_INI_DIRECTORY_REFUSED;
        if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) return HOGS_INI_PATH_ERROR;
        if (data.nFileSizeHigh != 0 || data.nFileSizeLow > 65536u) return HOGS_INI_TOO_LARGE;
        *exists = 1;
        *out_size = data.nFileSizeLow;
    }
    strncpy(parent, path, sizeof(parent)-1);
    parent[sizeof(parent)-1] = 0;
    slash = strrchr(parent, '\\');
    if (!slash) slash = strrchr(parent, '/');
    if (slash) {
        if (slash == parent) slash[1] = 0;
        else *slash = 0;
        attrs = GetFileAttributesA(parent);
        if (attrs == INVALID_FILE_ATTRIBUTES) return HOGS_INI_PATH_ERROR;
        if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) return HOGS_INI_REPARSE_REFUSED;
        if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) return HOGS_INI_PATH_ERROR;
    }
    return HOGS_INI_OK;
}

static __inline HogsIniResult hogs_ini_read_value(const char *path, const char *key, const char *fallback, int *out, int *was_present) {
    char buf[32];
    DWORD n = GetPrivateProfileStringA("Mouse", key, "__HOGS_MISSING__", buf, (DWORD)sizeof(buf), path);
    if (n >= sizeof(buf)-1) return HOGS_INI_MALFORMED;
    if (strcmp(buf, "__HOGS_MISSING__") == 0) {
        if (!hogs_ini_parse_int(fallback, out)) return HOGS_INI_MALFORMED;
        *was_present = 0;
        return HOGS_INI_OK;
    }
    if (!hogs_ini_parse_int(buf, out)) return HOGS_INI_MALFORMED;
    *was_present = 1;
    return HOGS_INI_OK;
}

static __inline HogsIniResult hogs_ini_load(const char *path, HogsMouseSettings *out) {
    DWORD size;
    int exists, present, any = 0;
    HogsIniResult r;
    HogsMouseSettings s = hogs_ini_defaults();
    char fallback[16];
    if (!out) return HOGS_INI_BAD_ARGUMENT;
    r = hogs_ini_check_path(path, &size, &exists);
    if (r != HOGS_INI_OK) return r;
    if (!exists) { *out = s; return HOGS_INI_OK; }
#define READ_KEY(name, field) do { \
    _snprintf(fallback, sizeof(fallback), "%d", s.field); fallback[sizeof(fallback)-1]=0; \
    r = hogs_ini_read_value(path, name, fallback, &s.field, &present); \
    if (r != HOGS_INI_OK) return r; any |= present; \
} while (0)
    READ_KEY("NativeUnitsPerPixel", native_units_per_pixel);
    READ_KEY("AimSensitivityPercent", aim_sensitivity_percent);
    READ_KEY("InvertX", invert_x);
    READ_KEY("InvertY", invert_y);
#undef READ_KEY
    (void)any;
    if (!hogs_ini_validate(&s)) return HOGS_INI_MALFORMED;
    *out = s;
    return HOGS_INI_OK;
}

static __inline int hogs_ini_make_sidepath(const char *path, char *out, size_t cap, const char *tag, unsigned attempt) {
    const char *slash1 = strrchr(path, '\\');
    const char *slash2 = strrchr(path, '/');
    const char *slash = slash1;
    if (!slash || (slash2 && slash2 > slash)) slash = slash2;
    size_t dirlen = slash ? (size_t)(slash - path + 1) : 0;
    int n = _snprintf(out, cap, "%.*sHogsMouse.%s.%08lx.%08lx.tmp", (int)dirlen, path,
                      tag, (unsigned long)GetCurrentProcessId(), (unsigned long)(GetTickCount() + attempt));
    return n > 0 && (size_t)n < cap;
}

static __inline int hogs_ini_write_values(const char *path, const HogsMouseSettings *s) {
    char value[16];
#define PUT(k, v) do { _snprintf(value, sizeof(value), "%d", (v)); value[sizeof(value)-1]=0; if (!WritePrivateProfileStringA("Mouse", k, value, path)) return 0; } while (0)
    PUT("NativeUnitsPerPixel", s->native_units_per_pixel);
    PUT("AimSensitivityPercent", s->aim_sensitivity_percent);
    PUT("InvertX", s->invert_x);
    PUT("InvertY", s->invert_y);
#undef PUT
    /* A NULL triplet flushes the profile cache; false is the documented flush result. */
    WritePrivateProfileStringA(NULL, NULL, NULL, path);
    return 1;
}

static __inline int hogs_ini_same(const HogsMouseSettings *a, const HogsMouseSettings *b) {
    return a->native_units_per_pixel == b->native_units_per_pixel &&
           a->aim_sensitivity_percent == b->aim_sensitivity_percent &&
           a->invert_x == b->invert_x && a->invert_y == b->invert_y;
}

static __inline int hogs_ini_files_equal(const char *a, const char *b) {
    HANDLE ha, hb;
    BYTE ba[2048], bb[2048];
    DWORD na, nb;
    int equal = 1;
    ha = CreateFileA(a, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                     NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (ha == INVALID_HANDLE_VALUE) return 0;
    hb = CreateFileA(b, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                     NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hb == INVALID_HANDLE_VALUE) { CloseHandle(ha); return 0; }
    do {
        na = nb = 0;
        if (!ReadFile(ha, ba, sizeof(ba), &na, NULL) || !ReadFile(hb, bb, sizeof(bb), &nb, NULL) ||
            na != nb || memcmp(ba, bb, na) != 0) { equal = 0; break; }
    } while (na != 0);
    CloseHandle(hb); CloseHandle(ha);
    return equal;
}

static __inline int hogs_ini_matches_snapshot(const char *path, const char *snapshot, int existed) {
    DWORD attrs = GetFileAttributesA(path);
    if (existed) {
        if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) return 0;
        return hogs_ini_files_equal(path, snapshot);
    }
    if (attrs != INVALID_FILE_ATTRIBUTES) return 0;
    if (GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_PATH_NOT_FOUND) return 0;
    /* Also detect a dangling reparse entry that GetFileAttributes cannot follow. */
    {
        HANDLE h = CreateFileA(path, FILE_READ_ATTRIBUTES,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING,
                               FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); return 0; }
        if (GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_PATH_NOT_FOUND) return 0;
    }
    return 1;
}

#ifdef HOGSMOUSE_TEST
static __inline int hogs_ini_test_mutate_path(const char *path, int existed) {
    static const char replacement[] = "; external concurrent writer\r\n";
    HANDLE h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                           existed ? CREATE_ALWAYS : CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written = 0;
    int ok;
    if (h == INVALID_HANDLE_VALUE) return 0;
    ok = WriteFile(h, replacement, sizeof(replacement)-1, &written, NULL) && written == sizeof(replacement)-1;
    CloseHandle(h);
    return ok;
}
#endif

static __inline HogsIniResult hogs_ini_save_impl(const char *path, const HogsMouseSettings *s, unsigned test_failures) {
    char temp[MAX_PATH], backup[MAX_PATH];
    DWORD size;
    int exists;
    HogsIniResult r;
    HogsMouseSettings check;
    HANDLE h;
    int made_temp = 0, made_backup = 0, committed = 0;
    unsigned i;
#ifndef HOGSMOUSE_TEST
    (void)test_failures;
#endif
    if (!hogs_ini_validate(s)) return HOGS_INI_BAD_ARGUMENT;
    r = hogs_ini_check_path(path, &size, &exists);
    if (r != HOGS_INI_OK) return r;
    for (i=0; i<64; ++i) {
        if (!hogs_ini_make_sidepath(path, temp, sizeof(temp), "write", i)) return HOGS_INI_PATH_ERROR;
        h = CreateFileA(temp, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, NULL);
        if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); made_temp=1; break; }
        if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) return HOGS_INI_IO_ERROR;
    }
    if (!made_temp) return HOGS_INI_IO_ERROR;
    if (exists) {
        if (!CopyFileA(path, temp, FALSE)) { DeleteFileA(temp); return HOGS_INI_IO_ERROR; }
        for (i=0; i<64; ++i) {
            if (!hogs_ini_make_sidepath(path, backup, sizeof(backup), "rollback", i)) { DeleteFileA(temp); return HOGS_INI_PATH_ERROR; }
            if (CopyFileA(path, backup, TRUE)) { made_backup=1; break; }
            if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) { DeleteFileA(temp); return HOGS_INI_IO_ERROR; }
        }
        if (!made_backup) { DeleteFileA(temp); return HOGS_INI_IO_ERROR; }
    }
    if (!hogs_ini_write_values(temp, s)) { r=HOGS_INI_PROFILE_WRITE_FAILED; goto fail_precommit; }
    if (hogs_ini_load(temp, &check) != HOGS_INI_OK || !hogs_ini_same(&check, s)) { r=HOGS_INI_READBACK_FAILED; goto fail_precommit; }
#ifdef HOGSMOUSE_TEST
    if (test_failures & HOGS_INI_TEST_FAIL_PRECOMMIT) { r=HOGS_INI_COMMIT_FAILED; goto fail_precommit; }
    if ((test_failures & HOGS_INI_TEST_MUTATE_BEFORE_COMMIT) && !hogs_ini_test_mutate_path(path, exists)) {
        r=HOGS_INI_IO_ERROR; goto fail_precommit;
    }
#endif
    if (!hogs_ini_matches_snapshot(path, backup, exists)) { r=HOGS_INI_COMMIT_FAILED; goto fail_precommit; }
    if (!MoveFileExA(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { r=HOGS_INI_COMMIT_FAILED; goto fail_precommit; }
    made_temp=0;
    committed=1;
#ifdef HOGSMOUSE_TEST
    if (test_failures & HOGS_INI_TEST_FAIL_POSTCOMMIT) { r=HOGS_INI_READBACK_FAILED; goto fail_postcommit; }
#endif
    WritePrivateProfileStringA(NULL, NULL, NULL, path);
    if (hogs_ini_load(path, &check) != HOGS_INI_OK || !hogs_ini_same(&check, s)) { r=HOGS_INI_READBACK_FAILED; goto fail_postcommit; }
    if (made_backup) DeleteFileA(backup);
    return HOGS_INI_OK;
fail_postcommit:
    if (committed) {
        if (exists) {
            char restore[MAX_PATH];
            int restored = 0;
            for (i=0; i<64; ++i) {
                if (!hogs_ini_make_sidepath(path, restore, sizeof(restore), "restore", i)) return HOGS_INI_ROLLBACK_FAILED;
                if (CopyFileA(backup, restore, TRUE)) { restored=1; break; }
                if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) return HOGS_INI_ROLLBACK_FAILED;
            }
            if (!restored || !MoveFileExA(restore, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                if (restored) DeleteFileA(restore);
                return HOGS_INI_ROLLBACK_FAILED;
            }
        } else {
            if (!DeleteFileA(path) && GetLastError() != ERROR_FILE_NOT_FOUND) return HOGS_INI_ROLLBACK_FAILED;
        }
        WritePrivateProfileStringA(NULL, NULL, NULL, path);
        if (exists) {
            HANDLE original = CreateFileA(backup, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            HANDLE restored_file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            BYTE a[4096], b[4096];
            DWORD na, nb;
            int same = original != INVALID_HANDLE_VALUE && restored_file != INVALID_HANDLE_VALUE;
            if (same) {
                do {
                    na = nb = 0;
                    if (!ReadFile(original, a, sizeof(a), &na, NULL) || !ReadFile(restored_file, b, sizeof(b), &nb, NULL) || na != nb || memcmp(a,b,na)) { same=0; break; }
                } while (na != 0);
            }
            if (original != INVALID_HANDLE_VALUE) CloseHandle(original);
            if (restored_file != INVALID_HANDLE_VALUE) CloseHandle(restored_file);
            if (!same) return HOGS_INI_ROLLBACK_FAILED;
        }
    }
    if (made_backup) DeleteFileA(backup);
    return r;
fail_precommit:
    if (made_temp) DeleteFileA(temp);
    if (made_backup) DeleteFileA(backup);
    return r;
}

static __inline HogsIniResult hogs_ini_save(const char *path, const HogsMouseSettings *s) {
    return hogs_ini_save_impl(path, s, 0);
}
#ifdef HOGSMOUSE_TEST
static __inline HogsIniResult hogs_ini_save_test(const char *path, const HogsMouseSettings *s, const HogsIniOps *ops) {
    return hogs_ini_save_impl(path, s, ops ? ops->test_failures : 0);
}
#endif

#ifdef __cplusplus
}
#endif
#endif
