#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include "native_input_policy.h"

#define T(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while (0)
int main(void) {
    HNPacket p = {0}; uint32_t axes = 0xdeadbeef, mask = 0xdeadbeef;
    for (int dy = -256; dy <= 256; ++dy) {
        hn_stage(&p, 7, 1000, HN_BODY, 5, dy, 2, 0);
        T(hn_take(&p,7,1000,0,0x1020,&axes,&mask));
        T(axes == 10 && mask == 0x1020); /* Never move from mouse Y. */
        T(!hn_take(&p,7,1000,0,0,&axes,&mask));
    }
    puts("PASS 513 body frames never map vertical mouse movement into walking; each is consumed once");
    hn_stage(&p,7,1000,HN_BODY,-5,0,2,0);
    T(hn_take(&p,7,1000,0,0,&axes,&mask)); T((int8_t)axes == -10);
    hn_stage(&p,7,1000,HN_BODY,0,0,2,0); T(!p.ready);
    puts("PASS direction reversal is immediate; idle creates no queued movement");
    hn_stage(&p,7,1000,HN_BODY,5,30,2,0);
    T(hn_take(&p,7,1000,0,0x8021,&axes,&mask));
    T((int8_t)axes==10 && (int8_t)(axes>>8)==-126 && mask==0x8020);
    hn_stage(&p,7,1000,HN_BODY,5,0,2,0);
    T(hn_take(&p,7,1000,0,0x202a,&axes,&mask));
    T((int8_t)axes==64 && (int8_t)(axes>>8)==126 && mask==0x2020);
    puts("PASS keyboard walk and turn coexist through native axes; action bits preserved");
    hn_stage(&p,7,1000,HN_AIM,3,-4,2,0);
    T(hn_take(&p,7,1000,0,0x120,&axes,&mask));
    T((int8_t)axes==6 && (int8_t)(axes>>8)==-8 && mask==0x120);
    hn_stage(&p,7,1000,HN_AIM,3,-4,2,1);
    T(hn_take(&p,7,1000,0,0x120,&axes,&mask)); T((int8_t)(axes>>8)==8);
    puts("PASS independent signed aim axes and configured inversion");
    hn_stage(&p,7,1000,HN_AIM,30,40,2,0);
    T(!hn_take(&p,7,1000,0,1,&axes,&mask)); T(!p.ready);
    hn_stage(&p,7,1000,HN_BODY,30,40,2,0);
    T(!hn_take(&p,7,1000,0x12345678,0,&axes,&mask)); T(!p.ready);
    puts("PASS explicit keyboard aim and existing joystick input retain ownership");
    hn_stage(&p,7,1000,HN_BODY,5,0,2,0);
    T(!hn_take(&p,8,1000,0,0,&axes,&mask));
    hn_stage(&p,7,1000,HN_BODY,5,0,2,0);
    T(!hn_take(&p,7,1101,0,0,&axes,&mask));
    hn_stage(&p,7,UINT32_MAX-20,HN_BODY,5,0,2,0);
    T(hn_take(&p,7,10,0,0,&axes,&mask));
    hn_stage(&p,7,1000,HN_BODY,5,0,2,0); hn_clear(&p);
    T(!hn_take(&p,7,1000,0,0,&axes,&mask));
    puts("PASS player mismatch, stale frames, lifecycle reset and timer wraparound");
    T(hn_axis(INT_MAX,8)==120 && hn_axis(INT_MIN,8)==-120);
    hn_stage(&p,7,1000,HN_AIM,INT_MIN,INT_MIN,8,1);
    T(p.x==-120 && p.y==120);
    puts("PASS bounded native axis range and integer edge cases");
    return 0;
}
