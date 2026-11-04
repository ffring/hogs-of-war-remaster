#ifndef HOGS_MOUSE_TOKENS_H
#define HOGS_MOUSE_TOKENS_H
#include <stdint.h>
#include <string.h>
enum { HMT_CAPACITY=16, HMT_BODY=1, HMT_AIM=2 };
typedef struct {
    uint32_t user,pig,actor,camera,weapon,mode,wire,at;
    int route,x,y;
} HMTToken;
typedef struct { HMTToken rows[HMT_CAPACITY];unsigned count; } HMTokenRing;
static inline void hmt_clear(HMTokenRing *r) { memset(r,0,sizeof(*r)); }
static inline void hmt_push(HMTokenRing *r,const HMTToken *t) {
    if (!r || !t || !t->user || !t->pig || !t->actor || !t->camera || !t->wire ||
        (t->route!=HMT_BODY && t->route!=HMT_AIM)) return;
    if (r->count>=HMT_CAPACITY) {
        memmove(r->rows,r->rows+1,(HMT_CAPACITY-1)*sizeof(*r->rows));r->count=HMT_CAPACITY-1;
    }
    r->rows[r->count++]=*t;
}
static inline int hmt_take(HMTokenRing *r,uint32_t now,const HMTToken *key,HMTToken *out) {
    if (!r || !key || !out) return 0;
    unsigned kept=0;
    for (unsigned i=0;i<r->count;++i)
        if ((uint32_t)(now-r->rows[i].at)<=100) r->rows[kept++]=r->rows[i];
    r->count=kept;
    for (unsigned i=0;i<r->count;++i) {
        HMTToken *t=&r->rows[i];
        if (t->user==key->user && t->pig==key->pig && t->actor==key->actor &&
            t->camera==key->camera && t->weapon==key->weapon && t->mode==key->mode &&
            t->route==key->route && t->wire==key->wire) {
            *out=*t;
            /* Input order is native FIFO. Superseded unmatched rows cannot
               reappear as an unrelated later mouse gesture. */
            unsigned left=r->count-i-1;
            memmove(r->rows,r->rows+i+1,left*sizeof(*r->rows));r->count=left;
            return 1;
        }
    }
    return 0;
}
#endif
