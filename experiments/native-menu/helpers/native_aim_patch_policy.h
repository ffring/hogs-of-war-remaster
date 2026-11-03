#ifndef HOGS_NATIVE_AIM_PATCH_POLICY_H
#define HOGS_NATIVE_AIM_PATCH_POLICY_H

#include <stdint.h>
#include <string.h>

enum { HMNA_SITE_VA=0x00495690u, HMNA_STOLEN=8 };
static const uint8_t hmna_original[HMNA_STOLEN]={0x83,0xEC,0x10,0x53,0x55,0x56,0x8B,0xF1};

static inline int hmna_identity(uintptr_t base, uint16_t machine,
                                uint32_t preferred_base, uint32_t timestamp,
                                uint32_t image_size) {
    return base==0x00400000u && machine==0x014cu && preferred_base==0x00400000u &&
           timestamp==0x3a069f75u && image_size==0x0015e000u;
}

static inline int hmna_jump(uintptr_t site, uintptr_t target, uint8_t patch[HMNA_STOLEN]) {
    int64_t rel=(int64_t)target-((int64_t)site+5);
    if(!patch || rel<INT32_MIN || rel>INT32_MAX) return 0;
    int32_t disp=(int32_t)rel;
    patch[0]=0xe9; memcpy(patch+1,&disp,4); patch[5]=patch[6]=patch[7]=0x90;
    return 1;
}

static inline int hmna_trampoline(uintptr_t site, uintptr_t trampoline,
                                  uint8_t out[HMNA_STOLEN+5]) {
    int64_t rel=(int64_t)(site+HMNA_STOLEN)-((int64_t)trampoline+HMNA_STOLEN+5);
    if(!out || rel<INT32_MIN || rel>INT32_MAX) return 0;
    memcpy(out,hmna_original,HMNA_STOLEN);
    out[HMNA_STOLEN]=0xe9;
    int32_t disp=(int32_t)rel;
    memcpy(out+HMNA_STOLEN+1,&disp,4);
    return 1;
}

static inline int hmna_original_matches(const uint8_t *site) {
    return site && memcmp(site,hmna_original,HMNA_STOLEN)==0;
}

static inline int hmna_pig_yaw_slot(uint32_t vtable,uint32_t target) {
    return vtable==0x004bd298u && target==0x0046af30u;
}

#endif
