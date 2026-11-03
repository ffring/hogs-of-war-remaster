#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../helpers/native_aim_patch_policy.h"

static uint32_t rel32(const uint8_t *p) { uint32_t v; memcpy(&v,p,4); return v; }

int main(void) {
    uint8_t site[8], patch[HMNA_STOLEN], tramp[HMNA_STOLEN+5];
    memcpy(site,hmna_original,sizeof(site));
    assert(hmna_identity(0x400000,0x14c,0x400000,0x3a069f75,0x15e000));
    assert(!hmna_identity(0x400000,0x8664,0x400000,0x3a069f75,0x15e000));
    assert(hmna_original_matches(site));
    assert(!hmna_original_matches(site+1));
    assert(hmna_jump(0x495690,0x70000000,patch));
    assert(patch[0]==0xe9 && patch[5]==0x90 && patch[6]==0x90 && patch[7]==0x90);
    assert((int64_t)0x495695+(int32_t)rel32(patch+1)==0x70000000);
    assert(hmna_trampoline(0x495690,0x60000000,tramp));
    assert(memcmp(tramp,hmna_original,HMNA_STOLEN)==0 && tramp[8]==0xe9);
    assert((int64_t)0x6000000d+(int32_t)rel32(tramp+9)==0x495698);
    assert(hmna_pig_yaw_slot(0x4bd298,0x46af30));
    assert(!hmna_pig_yaw_slot(0x4bd298,0x478b40));
    puts("PASS: exact image/site guards, x86 jump/trampoline targets and native yaw slot");
    return 0;
}
