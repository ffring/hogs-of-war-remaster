#ifndef HOGS_BODY_YAW_POLICY_H
#define HOGS_BODY_YAW_POLICY_H
#include <stdint.h>

typedef struct { int32_t fraction; } HNBodyYawScale;

static inline void hn_body_yaw_reset(HNBodyYawScale *scale) {
    if (scale) scale->fraction=0;
}

/* Convert the existing synchronized body-turn wire value into the requested
   local multiplier. Fractional quarters accumulate only across same-direction
   input; an idle frame emits nothing and a sign reversal drops old debt. */
static inline int32_t hn_body_yaw_step(HNBodyYawScale *scale,int32_t dx,int gain) {
    if (!scale) return 0;
    if (gain<1) gain=1;
    if (gain>24) gain=24;
    if ((dx>0 && scale->fraction<0) || (dx<0 && scale->fraction>0))
        scale->fraction=0;
    int64_t total=(int64_t)dx*gain+scale->fraction;
    int64_t whole=total/4;
    int64_t rem=total%4;
    int32_t cap=(30*gain)/4;
    if (whole>cap) { whole=cap;rem=0; }
    if (whole< -cap) { whole= -cap;rem=0; }
    scale->fraction=(int32_t)rem;
    return (int32_t)whole;
}
#endif
