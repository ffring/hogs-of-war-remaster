#ifndef HOGS_SIGHT_PATCH_CORE_H
#define HOGS_SIGHT_PATCH_CORE_H

#include <stdint.h>
#include <stddef.h>

static inline void *hsp_memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    size_t i;
    for (i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}
static inline int hsp_memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *x = (const uint8_t *)a;
    const uint8_t *y = (const uint8_t *)b;
    size_t i;
    for (i = 0; i < n; ++i) if (x[i] != y[i]) return (int)x[i] - (int)y[i];
    return 0;
}
#define memcpy hsp_memcpy
#define memcmp hsp_memcmp

#define HSP_EXE_BASE 0x00400000u
#define HSP_EXE_TIMESTAMP 0x3A069F75u
#define HSP_EXE_IMAGE_SIZE 0x0015E000u
#define HSP_SITE_RVA (0x004549ADu - HSP_EXE_BASE)
#define HSP_STRING_RVA (0x004CF974u - HSP_EXE_BASE)
#define HSP_ALIAS_BYTES 16u
#define HSP_CANDIDATE_SIZE 2672u
#define HSP_CANDIDATE_CRC32 0xB489FD73u

static const uint8_t hsp_expected_site[5] = {0x68, 0x74, 0xF9, 0x4C, 0x00};
static const uint8_t hsp_expected_string[HSP_ALIAS_BYTES] = {
    'c','h','a','r','s','\\','s','i','g','h','t','.','m','a','d',0
};
static const uint8_t hsp_replacement_string[HSP_ALIAS_BYTES] = {
    'h','o','g','s','\\','s','i','g','h','t','.','m','a','d',0,0
};

typedef struct HspIdentity {
    uintptr_t loaded_base;
    uint16_t machine;
    uint32_t preferred_base;
    uint32_t timestamp;
    uint32_t image_size;
} HspIdentity;

static inline int hsp_identity_ok(const HspIdentity *id) {
    return id && id->loaded_base == HSP_EXE_BASE &&
        id->machine == 0x014Cu && id->preferred_base == HSP_EXE_BASE &&
        id->timestamp == HSP_EXE_TIMESTAMP && id->image_size == HSP_EXE_IMAGE_SIZE;
}

static inline int hsp_preflight(const HspIdentity *id, const uint8_t *site,
                         const uint8_t *path_string, int alias_ok) {
    return hsp_identity_ok(id) && site && path_string && alias_ok &&
        memcmp(site, hsp_expected_site, sizeof(hsp_expected_site)) == 0 &&
        memcmp(path_string, hsp_expected_string, HSP_ALIAS_BYTES) == 0;
}

static inline uint32_t hsp_crc32(const uint8_t *p, size_t n) {
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;
    unsigned bit;
    for (i = 0; i < n; ++i) {
        crc ^= p[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1u));
    }
    return ~crc;
}

typedef int (*HspProtect)(void *ctx, void *address, size_t length,
                          unsigned new_protect, unsigned *old_protect);
typedef int (*HspFlush)(void *ctx, const void *address, size_t length);
typedef struct HspOps { HspProtect protect; HspFlush flush; void *ctx; } HspOps;

/* Same transactional write path is used by the DLL and the synthetic harness. */
static inline int hsp_patch_string(uint8_t *path_string, const HspOps *ops) {
    uint8_t before[HSP_ALIAS_BYTES];
    unsigned old_protect = 0, ignored = 0;
    if (!path_string || !ops || !ops->protect || !ops->flush ||
        memcmp(path_string, hsp_expected_string, sizeof(before)) != 0)
        return 0;
    memcpy(before, path_string, sizeof(before));
    if (!ops->protect(ops->ctx, path_string, sizeof(before), 0x40u, &old_protect))
        return 0;
    memcpy(path_string, hsp_replacement_string, sizeof(before));
    if (!ops->flush(ops->ctx, path_string, sizeof(before)) ||
        !ops->protect(ops->ctx, path_string, sizeof(before), old_protect, &ignored)) {
        memcpy(path_string, before, sizeof(before));
        (void)ops->flush(ops->ctx, path_string, sizeof(before));
        (void)ops->protect(ops->ctx, path_string, sizeof(before), old_protect, &ignored);
        return 0;
    }
    return 1;
}

#endif
