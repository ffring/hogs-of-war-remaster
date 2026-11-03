#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../tinywin_ops.h"

typedef struct Mock { int calls; int fail_restore; int fail_flush; } Mock;
static int mock_protect(void *ctx, void *address, size_t length,
                        unsigned mode, unsigned *old_mode) {
    Mock *m = (Mock *)ctx;
    (void)address; (void)length;
    ++m->calls;
    if (old_mode) *old_mode = (m->calls == 1 ? 0x20u : mode);
    if (m->fail_restore && m->calls == 2) return 0;
    return 1;
}
static int mock_flush(void *ctx, const void *address, size_t length) {
    Mock *m = (Mock *)ctx;
    (void)address; (void)length;
    return !m->fail_flush;
}
static int check(int okay, const char *what) {
    if (!okay) { fprintf(stderr, "FAIL: %s\n", what); return 0; }
    printf("PASS: %s\n", what); return 1;
}

int main(void) {
    uint8_t image[HSP_EXE_IMAGE_SIZE];
    uint8_t before[HSP_ALIAS_BYTES];
    HspIdentity id = { HSP_EXE_BASE, 0x014Cu, HSP_EXE_BASE,
                       HSP_EXE_TIMESTAMP, HSP_EXE_IMAGE_SIZE };
    HspOps ops;
    Mock mock;
    int ok = 1;
    memset(image, 0, sizeof(image));
    memcpy(image + HSP_SITE_RVA, hsp_expected_site, sizeof(hsp_expected_site));
    memcpy(image + HSP_STRING_RVA, hsp_expected_string, HSP_ALIAS_BYTES);
    ops.protect = mock_protect; ops.flush = mock_flush; ops.ctx = &mock;

    ok &= check(hsp_preflight(&id, image + HSP_SITE_RVA,
                              image + HSP_STRING_RVA, 1), "supported PE, exact site/string, alias verified");
    id.timestamp++;
    ok &= check(!hsp_preflight(&id, image + HSP_SITE_RVA,
                               image + HSP_STRING_RVA, 1), "wrong PE timestamp refused");
    id.timestamp--;
    ok &= check(!hsp_preflight(&id, image + HSP_SITE_RVA,
                               image + HSP_STRING_RVA, 0), "missing or invalid alias refused");
    image[HSP_SITE_RVA] ^= 1;
    ok &= check(!hsp_preflight(&id, image + HSP_SITE_RVA,
                               image + HSP_STRING_RVA, 1), "mixed or modified site bytes refused");
    image[HSP_SITE_RVA] ^= 1;
    image[HSP_STRING_RVA + 2] ^= 1;
    ok &= check(!hsp_preflight(&id, image + HSP_SITE_RVA,
                               image + HSP_STRING_RVA, 1), "mixed or modified path bytes refused");
    image[HSP_STRING_RVA + 2] ^= 1;

    memset(&mock, 0, sizeof(mock));
    ok &= check(hsp_patch_string(image + HSP_STRING_RVA, &ops) &&
                memcmp(image + HSP_STRING_RVA, hsp_replacement_string, HSP_ALIAS_BYTES) == 0,
                "guarded path patch succeeds");
    memcpy(image + HSP_STRING_RVA, hsp_expected_string, HSP_ALIAS_BYTES);

    memcpy(before, image + HSP_STRING_RVA, sizeof(before));
    memset(&mock, 0, sizeof(mock)); mock.fail_restore = 1;
    ok &= check(!hsp_patch_string(image + HSP_STRING_RVA, &ops) &&
                memcmp(image + HSP_STRING_RVA, before, sizeof(before)) == 0,
                "protection-restore failure rolls path bytes back");

    memset(&mock, 0, sizeof(mock)); mock.fail_flush = 1;
    ok &= check(!hsp_patch_string(image + HSP_STRING_RVA, &ops) &&
                memcmp(image + HSP_STRING_RVA, before, sizeof(before)) == 0,
                "cache-flush failure rolls path bytes back");

    {
        SYSTEM_INFO si;
        MEMORY_BASIC_INFORMATION mbi;
        uint8_t *page;
        DWORD old_mode = 0;
        HspOps real_ops = hsp_real_ops();
        GetSystemInfo(&si);
        page = (uint8_t *)VirtualAlloc(NULL, si.dwPageSize,
                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!page) return check(0, "VirtualAlloc readonly-page fixture");
        memcpy(page, hsp_expected_string, HSP_ALIAS_BYTES);
        if (!VirtualProtect(page, si.dwPageSize, PAGE_READONLY, &old_mode)) {
            VirtualFree(page, 0, MEM_RELEASE);
            return check(0, "VirtualProtect readonly-page fixture setup");
        }
        ok &= check(hsp_patch_string(page, &real_ops) &&
                    memcmp(page, hsp_replacement_string, HSP_ALIAS_BYTES) == 0,
                    "production C-ABI Win32 wrappers patch real readonly page");
        if (VirtualQuery(page, &mbi, sizeof(mbi)) != sizeof(mbi)) {
            VirtualFree(page, 0, MEM_RELEASE);
            return check(0, "VirtualQuery confirms restored protection");
        }
        ok &= check((mbi.Protect & 0xFFu) == PAGE_READONLY,
                    "production wrapper restored PAGE_READONLY");
        VirtualFree(page, 0, MEM_RELEASE);
    }
    return ok ? 0 : 1;
}
