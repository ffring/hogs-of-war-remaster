#include <assert.h>
#include "../helpers/mouse_tokens.h"
int main(void) {
    HMTokenRing ring={0};HMTToken base={1,2,3,4,7,14,120,1000,HMT_AIM,60,0},out;
    hmt_push(&ring,&base);base.x=90;hmt_push(&ring,&base);base.x=45;hmt_push(&ring,&base);
    assert(hmt_take(&ring,1001,&base,&out) && out.x==60);
    assert(hmt_take(&ring,1002,&base,&out) && out.x==90);
    HMTToken wrong=base;wrong.pig=5;assert(!hmt_take(&ring,1003,&wrong,&out));
    assert(hmt_take(&ring,1004,&base,&out) && out.x==45);
    assert(!hmt_take(&ring,1005,&base,&out));
    hmt_push(&ring,&base);assert(!hmt_take(&ring,1101,&base,&out) && !ring.count);
    base.at=0xfffffff0u;hmt_push(&ring,&base);assert(hmt_take(&ring,5,&base,&out));
    base.at=2000;hmt_push(&ring,&base);base.wire=124;hmt_push(&ring,&base);
    assert(hmt_take(&ring,2001,&base,&out) && !ring.count);
    base.wire=120;assert(!hmt_take(&ring,2002,&base,&out));
    for (int i=0;i<20;++i) { base.x=i;hmt_push(&ring,&base); }
    assert(ring.count==16 && hmt_take(&ring,2003,&base,&out) && out.x==4);
    hmt_clear(&ring);assert(!ring.count && !hmt_take(&ring,2004,&base,&out));
    return 0;
}
