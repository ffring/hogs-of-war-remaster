#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../helpers/linear_aim_policy.h"
#include "../helpers/linear_aim_wrapper.h"

typedef struct {
    uint32_t axes, mask;
    int context_ok, mutate_axis, mutate_upper;
    int original_calls, pitch_calls, yaw_calls;
    int32_t pitch, yaw;
    int invalidate_after_pitch;
} Fixture;

static int context_ok(void *p) {
    Fixture *f=(Fixture *)p;
    return f->context_ok && (f->axes&0xffffu)==0xf406u;
}
static int32_t original(void *p) {
    Fixture *f=(Fixture *)p;
    f->original_calls++;
    assert((f->axes&0xffffu)==0);
    if (f->mutate_axis) f->axes|=1u;
    if (f->mutate_upper) f->axes=(f->axes&0xffffu)|0x5a6b0000u;
    return 0x1234;
}
static void pitch(void *p,int32_t d) { Fixture *f=(Fixture *)p; f->pitch_calls++; f->pitch=d; if(f->invalidate_after_pitch) f->context_ok=0; }
static void yaw(void *p,int32_t d) { Fixture *f=(Fixture *)p; f->yaw_calls++; f->yaw=d; }
static HMLCall make_call(Fixture *f,int mode,int weapon,int kind) {
    HMLCall c={0};
    c.mode=mode;c.weapon=weapon;c.controller_kind=kind;
    c.axes_word=&f->axes;c.button_mask=&f->mask;c.opaque=f;
    c.context_unchanged=context_ok;c.original_handler=original;
    c.pitch_delta=pitch;c.yaw_delta=yaw;
    return c;
}

int main(void) {
    HMLAxisPacket packet={6,-12};
    Fixture f={0xa5c3f406u,0,1,0,0,0,0,0,0,0,0};
    HMLCall c=make_call(&f,14,7,HML_CONTROLLER_HUMAN);
    assert(hml_dispatch(&c,packet)==HML_AIM_APPLIED);
    assert(f.original_calls==1&&f.pitch_calls==1&&f.pitch==-6&&f.yaw_calls==1&&f.yaw==3);
    assert(f.axes==0xa5c3f406u);

    f=(Fixture){0xa5c3f406u,0,1,0,1,0,0,0,0,0,0};c=make_call(&f,14,7,HML_CONTROLLER_HUMAN);
    assert(hml_dispatch(&c,packet)==HML_AIM_APPLIED);
    assert(f.axes==0x5a6bf406u); /* unrelated upper axes survive the actual low-axis context check */

    f=(Fixture){0xa5c3f406u,0,1,1,0,0,0,0,0,0,0};c=make_call(&f,14,7,HML_CONTROLLER_HUMAN);
    assert(hml_dispatch(&c,packet)==HML_ORIGINAL_ONLY);
    assert(f.original_calls==1&&f.pitch_calls==0&&f.yaw_calls==0&&f.axes==0xa5c30001u);

    f=(Fixture){0xa5c3f406u,0,0,0,0,0,0,0,0,0,0};c=make_call(&f,14,7,HML_CONTROLLER_HUMAN);
    assert(hml_dispatch(&c,packet)==HML_NOT_INTERCEPTED&&f.original_calls==0);
    f.context_ok=1;f.mask=1;
    assert(hml_dispatch(&c,packet)==HML_NOT_INTERCEPTED&&f.original_calls==0);

    f=(Fixture){0xa5c3f406u,0,1,0,0,0,0,0,0,0,0};c=make_call(&f,18,7,HML_CONTROLLER_HUMAN);
    assert(hml_dispatch(&c,packet)==HML_NOT_INTERCEPTED&&f.original_calls==0);
    c=make_call(&f,14,11,HML_CONTROLLER_HUMAN);
    assert(hml_dispatch(&c,packet)==HML_NOT_INTERCEPTED&&f.original_calls==0);
    c=make_call(&f,14,7,2);
    assert(hml_dispatch(&c,packet)==HML_NOT_INTERCEPTED&&f.original_calls==0);

    /* Every ordinary weapon in the two retail modes shares the validated
       linear sinks; only the two dedicated camera/zoom IDs are refused. */
    for (int mode=14;mode<=17;mode+=3) {
        for (int weapon=1;weapon<=80;++weapon) {
            f=(Fixture){0xa5c3f406u,0,1,0,0,0,0,0,0,0,0};
            c=make_call(&f,mode,weapon,HML_CONTROLLER_HUMAN);
            int result=hml_dispatch(&c,packet);
            if (weapon==11 || weapon==64)
                assert(result==HML_NOT_INTERCEPTED&&f.original_calls==0);
            else
                assert(result==HML_AIM_APPLIED&&f.original_calls==1&&
                       f.pitch_calls==1&&f.yaw_calls==1);
        }
    }
    f=(Fixture){0xa5c3f406u,0,1,0,0,0,0,0,0,0,0};
    c=make_call(&f,14,0,HML_CONTROLLER_HUMAN);
    assert(hml_dispatch(&c,packet)==HML_NOT_INTERCEPTED);
    c=make_call(&f,17,81,HML_CONTROLLER_HUMAN);
    assert(hml_dispatch(&c,packet)==HML_NOT_INTERCEPTED);

    packet=(HMLAxisPacket){0,0};c=make_call(&f,14,7,HML_CONTROLLER_HUMAN);
    assert(hml_dispatch(&c,packet)==HML_NOT_INTERCEPTED&&f.original_calls==0);

    f=(Fixture){0xa5c3f406u,0,1,0,0,0,0,0,0,1,1};
    c=make_call(&f,14,7,HML_CONTROLLER_HUMAN);
    assert(hml_dispatch(&c,(HMLAxisPacket){6,-12})==HML_ORIGINAL_ONLY);
    assert(f.original_calls==1&&f.pitch_calls==1&&f.yaw_calls==0);
    puts("PASS: native aim wrapper covers modes 14/17 and weapons 1..80 except the two original camera branches");
    return 0;
}
