#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "sight_patch_core.h"

#include "tinywin_ops.h"

static int hsp_same_path(const char *a, const char *b) {
    unsigned char ca, cb;
    size_t i = 0, na = 0, nb = 0;
    while (a[na]) ++na;
    while (b[nb]) ++nb;
    while (na > 3 && (a[na - 1] == '\\' || a[na - 1] == '/')) --na;
    while (nb > 3 && (b[nb - 1] == '\\' || b[nb - 1] == '/')) --nb;
    if (na != nb) return 0;
    while (i < na) {
        ca = (unsigned char)a[i]; cb = (unsigned char)b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb + ('a' - 'A'));
        if (ca != cb) return 0;
        ++i;
    }
    return 1;
}

static int hsp_check_alias_file(void) {
    char path[MAX_PATH], game_root[MAX_PATH], cwd[MAX_PATH];
    DWORD length, slash = 0, i, size_low, size_high = 0, got = 0;
    HANDLE file;
    uint8_t contents[HSP_CANDIDATE_SIZE];
    HspOps ops;
    HMODULE exe = GetModuleHandleA(NULL);
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS32 *nt;
    uint8_t *base, *site, *name;
    HspIdentity identity;
    int ok = 0;

    if (!exe) return 0;
    base = (uint8_t *)exe;
    dos = (IMAGE_DOS_HEADER *)base;
    if ((uintptr_t)base != HSP_EXE_BASE || dos->e_magic != IMAGE_DOS_SIGNATURE ||
        dos->e_lfanew <= 0 || dos->e_lfanew > 0x1000) return 0;
    nt = (IMAGE_NT_HEADERS32 *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) return 0;
    identity.loaded_base = (uintptr_t)base;
    identity.machine = nt->FileHeader.Machine;
    identity.preferred_base = nt->OptionalHeader.ImageBase;
    identity.timestamp = nt->FileHeader.TimeDateStamp;
    identity.image_size = nt->OptionalHeader.SizeOfImage;
    site = base + HSP_SITE_RVA;
    name = base + HSP_STRING_RVA;
    if (!hsp_identity_ok(&identity) ||
        memcmp(site, hsp_expected_site, sizeof(hsp_expected_site)) != 0 ||
        memcmp(name, hsp_expected_string, HSP_ALIAS_BYTES) != 0) return 0;

    length = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (!length || length >= MAX_PATH) return 0;
    for (i = 0; i < length; ++i)
        if (path[i] == '\\' || path[i] == '/') slash = i + 1;
    if (!slash || slash + sizeof("hogs\\sight.mad") > MAX_PATH) return 0;
    memcpy(game_root, path, slash);
    game_root[slash] = 0;
    length = GetCurrentDirectoryA(MAX_PATH, cwd);
    if (!length || length >= MAX_PATH || !hsp_same_path(game_root, cwd)) return 0;
    memcpy(path + slash, "hogs\\sight.mad", sizeof("hogs\\sight.mad"));

    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    size_low = GetFileSize(file, &size_high);
    if (size_low == HSP_CANDIDATE_SIZE && size_high == 0 &&
        ReadFile(file, contents, HSP_CANDIDATE_SIZE, &got, NULL) &&
        got == HSP_CANDIDATE_SIZE &&
        hsp_crc32(contents, HSP_CANDIDATE_SIZE) == HSP_CANDIDATE_CRC32) ok = 1;
    CloseHandle(file);
    if (!ok || !hsp_preflight(&identity, site, name, 1)) return 0;

    ops = hsp_real_ops();
    return hsp_patch_string(name, &ops);
}

static void hsp_log_status(int patched) {
    char path[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL, path, MAX_PATH), slash = 0, i, wrote = 0;
    HANDLE log_file;
    const char *line = patched ? "validated alternate SIGHT path installed\r\n"
                               : "guards failed; original SIGHT path retained\r\n";
    DWORD line_size = 0;
    while (line[line_size]) ++line_size;
    if (!length || length >= MAX_PATH) return;
    for (i = 0; i < length; ++i)
        if (path[i] == '\\' || path[i] == '/') slash = i + 1;
    if (!slash || slash + sizeof("HogsSightPath.log") > MAX_PATH) return;
    memcpy(path + slash, "HogsSightPath.log", sizeof("HogsSightPath.log"));
    log_file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (log_file == INVALID_HANDLE_VALUE) return;
    (void)WriteFile(log_file, line, line_size, &wrote, NULL);
    CloseHandle(log_file);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        int patched;
        DisableThreadLibraryCalls(instance);
        patched = hsp_check_alias_file();
        hsp_log_status(patched);
        if (patched)
            OutputDebugStringA("HogsSightPath: validated alternate SIGHT mesh path installed.\n");
        else
            OutputDebugStringA("HogsSightPath: guards failed; original SIGHT path retained.\n");
    }
    return TRUE;
}
