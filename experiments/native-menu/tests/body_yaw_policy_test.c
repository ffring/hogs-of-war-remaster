#include <assert.h>
#include <stdio.h>
#include "../helpers/body_yaw_policy.h"

int main(void) {
    HNBodyYawScale s={0};
    assert(hn_body_yaw_step(&s,1,1)==0);
    assert(hn_body_yaw_step(&s,1,1)==0);
    assert(hn_body_yaw_step(&s,1,1)==0);
    assert(hn_body_yaw_step(&s,1,1)==1);
    assert(hn_body_yaw_step(&s,0,1)==0);
    assert(hn_body_yaw_step(&s,-1,1)==0); /* positive fractional debt discarded */
    assert(hn_body_yaw_step(&s,-1,1)==0);
    assert(hn_body_yaw_step(&s,-1,1)==0);
    assert(hn_body_yaw_step(&s,-1,1)==-1);
    hn_body_yaw_reset(&s);
    assert(hn_body_yaw_step(&s,1,12)==3);
    assert(hn_body_yaw_step(&s,100,12)==90);
    hn_body_yaw_reset(&s);
    assert(hn_body_yaw_step(&s,-100,24)==-180);
    hn_body_yaw_reset(&s);
    assert(hn_body_yaw_step(&s,1,0)==0);
    assert(hn_body_yaw_step(&s,3,0)==1);
    puts("PASS body yaw fractional accumulation, reversal, idle, clamp and gain bounds");
    return 0;
}
