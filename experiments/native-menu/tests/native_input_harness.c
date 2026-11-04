#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../helpers/mouse_tokens.h"

typedef void (*KB_TARGET)(void);
void hm_test_set_prior(KB_TARGET prior);
void hm_test_call_stub(void);
void hm_test_reset_state(void);
void hm_test_set_now(DWORD now);
void hm_test_set_buttons(BYTE buttons);
void hm_test_set_cursor(LONG x, LONG y, int valid);
void hm_test_set_foreground(int active);
void hm_test_set_native_focus(int active);
void hm_test_set_raw_registration_match(int matches);
void hm_test_poll_state(void);
void hm_test_set_ui(int active);
void hm_test_set_require_rmb(int enabled);
BYTE hm_test_run_callback(BYTE *buf, BYTE *user);
void hm_test_raw_button_event(USHORT flags);
void hm_test_raw_motion(LONG dx, LONG dy);
void hm_test_legacy_event(UINT msg, WPARAM wp, LPARAM lp);
BYTE hm_test_event_buttons_held(void);
BYTE hm_test_event_buttons_known(void);
BYTE hm_test_event_buttons_pressed(void);
BYTE hm_test_event_buttons_wait_release(void);
int hm_test_rearm_ready(void);
int hm_test_poll_dx(void);
int hm_test_mouse_dx(void);
int hm_test_is_armed(void);
void hm_test_queue_message(const MSG *m);

static BYTE *expected_buf, *expected_user;
static int prior_calls, prior_args_ok;
volatile BYTE prior_return;

static BYTE __cdecl mock_prior_c(BYTE *buf, BYTE *user) {
    prior_calls++;
    prior_args_ok = (buf == expected_buf && user == expected_user);
    return 0x5a;
}
static __declspec(naked) void mock_prior_stub(void) {
    __asm {
        push edi
        push dword ptr [esp + 8]
        call mock_prior_c
        add esp, 8
        ret
    }
}
static __declspec(naked) int __cdecl invoke_checked(BYTE *buf, BYTE *user) {
    __asm {
        push ebp
        mov ebp, esp
        push ebx
        push edi
        mov edi, dword ptr [ebp + 12]
        push dword ptr [ebp + 8]
        call hm_test_call_stub
        add esp, 4
        mov bl, al
        cmp edi, dword ptr [ebp + 12]
        sete al
        movzx eax, al
        mov byte ptr [prior_return], bl
        pop edi
        pop ebx
        pop ebp
        ret
    }
}

static int fail(const char *s) { fprintf(stderr, "FAIL %s\n", s); return 1; }
static void set_bindings(BYTE *user) {
    user[0x108 + 2*2] = 0x1e; /* Left */
    user[0x108 + 3*2] = 0x20; /* Right */
    user[0x108 + 8*2] = 0x1d; /* L1 */
    user[0x108 + 5*2] = 0x39; /* Cross */
}
static BYTE read_frame(BYTE *buf, BYTE *user) {
    memset(buf, 0, 256);
    expected_buf = buf; expected_user = user;
    return hm_test_run_callback(buf, user);
}
static int hasaim(const BYTE *b) { return (b[0x20] & 0x80) && (b[0x1d] & 0x80); }
static int has_direction(const BYTE *b) { return (b[0x20] & 0x80) || (b[0x1e] & 0x80) || (b[0x21] & 0x80) || (b[0x22] & 0x80); }

void hm_test_set_native_context(BYTE*,int,int,int);
void hm_test_apply_native(BYTE*,DWORD*,DWORD*);
int hm_test_native_ready(void);
int hm_test_merge(BYTE*,BYTE*);
void hm_test_set_native_kind(int);
void hm_test_set_local_game(int);
void hm_test_set_linear_aim_installed(int);
void hm_test_set_body_installed(int);
void hm_test_set_world(BYTE*,BYTE*,BYTE*);
void hm_test_body_apply(BYTE*,BYTE*,int32_t*);
int hm_test_body_logged(void);
unsigned hm_test_token_count(void);
int hm_test_take_token(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,int,int*,int*);
void hm_test_set_tuning(int,int,int,int);

static void ready(BYTE *b,BYTE *u,int weapon) {
    hm_test_reset_state();hm_test_set_native_context(u,6,weapon,4);read_frame(b,u);
}
static void ready_mode(BYTE *b,BYTE *u,int weapon,int mode) {
    hm_test_reset_state();hm_test_set_native_context(u,6,weapon,mode);read_frame(b,u);
}
static void setup_body_world(BYTE *user,BYTE *actor,BYTE *pig,BYTE *camera,DWORD *vtable) {
    memset(actor,0,0x514);memset(pig,0,0x308);memset(camera,0,0x88);memset(vtable,0,9*sizeof(DWORD));
    *(DWORD *)(user+0x10)=(DWORD)(uintptr_t)pig;
    *(DWORD *)pig=(DWORD)(uintptr_t)vtable;vtable[8]=0x0046af30;
    *(DWORD *)(actor+0x4fc)=(DWORD)(uintptr_t)user;
    *(DWORD *)(actor+0x510)=6;*(DWORD *)(actor+0x458)=0;
    *(DWORD *)(camera+0x84)=4;
    hm_test_set_world(actor,pig,camera);
}
#define T(c,m) do { if(!(c)) return fail(m); } while(0)
int main(void) {
 BYTE b[256],u[0x128],other[0x128];memset(u,0,sizeof u);memset(other,0,sizeof other);set_bindings(u);set_bindings(other);
 u[0x108]=0xc8;u[0x10a]=0xd0;u[0x10c]=0xcb;
 hm_test_set_prior(mock_prior_stub);expected_buf=b;expected_user=u;
 T(invoke_checked(b,u)&&prior_args_ok&&prior_calls==1&&prior_return==0x5a,"saved prior ABI");
 puts("PASS saved HogsFix ABI and EDI/result preserved");
 DWORD ax,mask;
 /* Verify the actual HogsMouse producer, packet publisher and body callback
    against a synthetic Steam-like world, not only the isolated ring policy. */
 {
   BYTE actor[0x514],pig[0x308],camera[0x88];DWORD vtable[9];int tx=0,ty=0;
   ready_mode(b,u,0,4);setup_body_world(u,actor,pig,camera,vtable);
   hm_test_set_tuning(12,300,1,1);hm_test_set_linear_aim_installed(1);hm_test_set_body_installed(1);
   hm_test_merge(b,u);hm_test_raw_motion(30,0);memset(b,0,sizeof b);hm_test_merge(b,u);
   ax=mask=0;hm_test_apply_native(u,&ax,&mask);
   T((int8_t)ax==120,"local body route changed the synchronized legacy wire axis");
   *(DWORD *)(actor+0x444)=ax;*(DWORD *)(actor+0x440)=mask;
   int32_t delta=(int32_t)(int8_t)(ax&0xffu)/4;
   T(delta==30,"fixture native body delta precondition");
   hm_test_body_apply(actor,pig,&delta);
   T(delta==90 && hm_test_body_logged(),"actual body callback did not replace native 30-unit step with bounded 90-unit local step");
   delta=30;hm_test_body_apply(actor,pig,&delta);
   T(delta==30,"body token replayed twice");
   puts("PASS producer→published legacy axis→identity-checked body callback applies one local 3x step");

   /* A second controller callback must leave the active token untouched; an
      actual joystick axis must consume/discard it instead of replaying later. */
   ready_mode(b,u,0,4);setup_body_world(u,actor,pig,camera,vtable);
   hm_test_set_tuning(12,300,1,1);hm_test_set_linear_aim_installed(1);hm_test_set_body_installed(1);
   hm_test_merge(b,u);hm_test_raw_motion(30,0);memset(b,0,sizeof b);hm_test_merge(b,u);
   ax=mask=0;hm_test_apply_native(u,&ax,&mask);
   T(hm_test_token_count()==1,"body token was not queued after successful native publish");
   hm_test_raw_motion(30,0);memset(b,0,sizeof b);hm_test_merge(b,u);
   { DWORD wrong_axes=0,wrong_mask=0;hm_test_apply_native(other,&wrong_axes,&wrong_mask); }
   T(hm_test_native_ready() && hm_test_token_count()==1,"other-controller callback consumed active body packet/token");
   ax=mask=0x0;hm_test_apply_native(u,&ax,&mask);
   T(hm_test_token_count()==2,"active callback could not append second body token");
   ax=120;mask=0;hm_test_apply_native(u,&ax,&mask); /* No pending packet; ignored. */
   T(hm_test_token_count()==2,"idle callback unexpectedly discarded valid queued body tokens");
   puts("PASS mismatched device poll preserves queued active-player token");

   /* A joystick-owned native axis rejects mouse overlay and invalidates tokens. */
   ready_mode(b,u,0,4);setup_body_world(u,actor,pig,camera,vtable);
   hm_test_set_tuning(12,300,1,1);hm_test_set_linear_aim_installed(1);hm_test_set_body_installed(1);
   hm_test_merge(b,u);hm_test_raw_motion(30,0);memset(b,0,sizeof b);hm_test_merge(b,u);
   ax=mask=0;hm_test_apply_native(u,&ax,&mask);T(hm_test_token_count()==1,"joystick fixture initial token missing");
   hm_test_raw_motion(30,0);memset(b,0,sizeof b);hm_test_merge(b,u);
   ax=120;mask=0;hm_test_apply_native(u,&ax,&mask);
   T(ax==120 && hm_test_token_count()==0,"joystick input did not win and clear stale body token");
   puts("PASS joystick ownership clears stale local body token");

   ready_mode(b,u,0,4);setup_body_world(u,actor,pig,camera,vtable);
   hm_test_set_tuning(12,300,1,1);hm_test_set_linear_aim_installed(1);hm_test_set_body_installed(1);
   hm_test_merge(b,u);hm_test_raw_motion(30,0);memset(b,0,sizeof b);hm_test_merge(b,u);
   ax=mask=0;hm_test_apply_native(u,&ax,&mask);T(hm_test_token_count()==1,"scope fixture initial token missing");
   hm_test_set_local_game(0);ax=mask=0;hm_test_apply_native(u,&ax,&mask);
   T(hm_test_token_count()==0,"DirectPlay state transition without a pending packet did not clear local tokens");
   puts("PASS local scope change clears pending local tokens even between mouse packets");

   ready_mode(b,u,0,4);setup_body_world(u,actor,pig,camera,vtable);
   hm_test_set_tuning(12,300,1,1);hm_test_set_linear_aim_installed(1);hm_test_set_body_installed(1);
   hm_test_merge(b,u);hm_test_raw_motion(30,0);memset(b,0,sizeof b);hm_test_merge(b,u);
   ax=0;mask=0x8;hm_test_apply_native(u,&ax,&mask);
   *(DWORD *)(actor+0x444)=ax;*(DWORD *)(actor+0x440)=mask;
   delta=(int32_t)(int8_t)(ax&0xffu)/4;hm_test_body_apply(actor,pig,&delta);
   T((int8_t)ax==64 && delta==16 && !hm_test_body_logged(),"keyboard LR did not retain ownership of body turn");
   puts("PASS keyboard left/right beats mouse body enhancement");

   ready_mode(b,u,0,4);setup_body_world(u,actor,pig,camera,vtable);
   hm_test_set_tuning(12,300,1,1);hm_test_set_linear_aim_installed(1);hm_test_set_body_installed(1);
   hm_test_set_local_game(0);hm_test_merge(b,u);hm_test_raw_motion(30,0);memset(b,0,sizeof b);hm_test_merge(b,u);
   ax=mask=0;hm_test_apply_native(u,&ax,&mask);
   *(DWORD *)(actor+0x444)=ax;*(DWORD *)(actor+0x440)=mask;
   delta=(int32_t)(int8_t)(ax&0xffu)/4;hm_test_body_apply(actor,pig,&delta);
   T((int8_t)ax==120 && delta==30 && !hm_test_body_logged(),"network state changed synchronized body input");
   puts("PASS active-network fake gate preserves synchronized body input");

   ready_mode(b,u,0,4);setup_body_world(u,actor,pig,camera,vtable);
   hm_test_set_tuning(12,300,1,1);hm_test_set_linear_aim_installed(1);hm_test_set_body_installed(1);
   hm_test_merge(b,u);hm_test_raw_motion(30,0);memset(b,0,sizeof b);hm_test_merge(b,u);
   ax=mask=0;hm_test_apply_native(u,&ax,&mask);
   *(DWORD *)(actor+0x444)=ax;*(DWORD *)(actor+0x440)=mask;
   hm_test_set_foreground(0);delta=(int32_t)(int8_t)(ax&0xffu)/4;
   hm_test_body_apply(actor,pig,&delta);
   T(delta==30 && !hm_test_body_logged(),"background body callback applied staged mouse yaw");
   puts("PASS focus loss prevents local body-token application");

   ready_mode(b,u,0,4);setup_body_world(u,actor,pig,camera,vtable);
   hm_test_set_tuning(12,300,1,1);hm_test_set_linear_aim_installed(1);hm_test_set_body_installed(1);
   hm_test_merge(b,u);hm_test_raw_motion(30,0);memset(b,0,sizeof b);hm_test_merge(b,u);
   ax=mask=0;hm_test_apply_native(u,&ax,&mask);
   *(DWORD *)(actor+0x444)=ax;*(DWORD *)(actor+0x440)=mask;
   hm_test_set_now(1201);delta=(int32_t)(int8_t)(ax&0xffu)/4;
   hm_test_body_apply(actor,pig,&delta);
   T(delta==30 && !hm_test_body_logged(),"expired body token applied after its 100ms window");
   puts("PASS stale local body token expires without changing native turn");
 }
 /* The producer and token ring cover every validated mode/weapon combination,
    and switch back to the synchronized keyboard path when DirectPlay is active. */
 ready_mode(b,u,1,14);hm_test_set_tuning(12,300,1,1);hm_test_set_linear_aim_installed(1);
 hm_test_merge(b,u);hm_test_raw_button_event(RI_MOUSE_RIGHT_BUTTON_DOWN);hm_test_merge(b,u);
 hm_test_raw_motion(8,-4);memset(b,0,sizeof b);hm_test_merge(b,u);ax=mask=0;hm_test_apply_native(u,&ax,&mask);
 { int tx=0,ty=0;T(ax!=0 && (b[0x1d]&0x80) && !has_direction(b),"mode14 aim didn't use native axis route");
   T(hm_test_take_token((uint32_t)(uintptr_t)u,0x3000,0x2000,0x1000,1,14,ax&0xffff,HMT_AIM,&tx,&ty),"mode14 aim token missing");
   T(tx==(int8_t)(ax&0xffu) && ty==(int8_t)((ax>>8)&0xffu),"mode14 aim token mismatch"); }
 ready_mode(b,u,80,17);hm_test_set_tuning(12,300,1,1);hm_test_set_linear_aim_installed(1);
 hm_test_merge(b,u);hm_test_raw_button_event(RI_MOUSE_RIGHT_BUTTON_DOWN);hm_test_merge(b,u);
 hm_test_raw_motion(8,0);memset(b,0,sizeof b);hm_test_merge(b,u);ax=mask=0;hm_test_apply_native(u,&ax,&mask);
 { int tx=0,ty=0;T(ax!=0 && hm_test_take_token((uint32_t)(uintptr_t)u,0x3000,0x2000,0x1000,80,17,ax&0xffff,HMT_AIM,&tx,&ty),"ordinary mode17/high weapon token missing"); }
 ready_mode(b,u,7,14);hm_test_set_tuning(12,300,1,1);hm_test_set_linear_aim_installed(1);hm_test_set_local_game(0);
 hm_test_merge(b,u);hm_test_raw_button_event(RI_MOUSE_RIGHT_BUTTON_DOWN);hm_test_merge(b,u);
 hm_test_raw_motion(40,0);memset(b,0,sizeof b);hm_test_merge(b,u);ax=mask=0;hm_test_apply_native(u,&ax,&mask);
 { int tx=0,ty=0;int took=hm_test_take_token((uint32_t)(uintptr_t)u,0x3000,0x2000,0x1000,7,14,0x0080,HMT_AIM,&tx,&ty);
   T(ax==0 && (b[0xcb]&0x80) && (b[0x1d]&0x80) && !took,"online session did not retain the existing inverted digital aim mapping"); }
 puts("PASS mode14/mode17 ordinary aim ring and synchronized online fallback");
 ready(b,u,0);hm_test_raw_motion(4,90);read_frame(b,u);
 T(hm_test_native_ready()&&!(b[0x1d]&0x80)&&!(b[0xc8]&0x80)&&!(b[0xd0]&0x80),"body motion became L1/walking keys");
 ax=mask=0;hm_test_apply_native(u,&ax,&mask);T(ax==16&&mask==0,"body input packet X/Y");
 ax=mask=0;hm_test_apply_native(u,&ax,&mask);T(ax==0,"body delta replayed");
 puts("PASS unarmed mouse X uses native body axis once; mouse Y never walks or opens orbit");
 ready(b,u,0);hm_test_raw_button_event(RI_MOUSE_RIGHT_BUTTON_DOWN);read_frame(b,u);
 hm_test_raw_motion(4,40);read_frame(b,u);ax=mask=0;hm_test_apply_native(u,&ax,&mask);
 T(ax==16&&!(b[0x1d]&0x80),"unarmed RMB still opens orbit");
 puts("PASS unarmed RMB does not inject free-orbit modifier");
 ready(b,u,7);hm_test_raw_button_event(RI_MOUSE_RIGHT_BUTTON_DOWN);read_frame(b,u);
 T((b[0x1d]&0x80)&&!hm_test_native_ready(),"weapon RMB did not hold native aim");
 hm_test_raw_motion(4,-4);read_frame(b,u);
 T((b[0x1d]&0x80)&&(b[0x20]&0x80)&&(b[0xc8]&0x80)&&!hm_test_native_ready(),"mode14 digital aim lost");
 read_frame(b,u);T((b[0x1d]&0x80)&&!(b[0x20]&0x80)&&!(b[0xc8]&0x80),"idle aim flicker or backlog");
 puts("PASS selected-weapon aiming preserves verified native key route including stationary hold");
 hm_test_raw_button_event(RI_MOUSE_LEFT_BUTTON_DOWN);
 for(DWORD t=1000;t<=2000;t+=40){hm_test_set_now(t);read_frame(b,u);T((b[0x39]&0x80)&&(b[0x1d]&0x80),"RMB+LMB charge lost");}
 hm_test_raw_button_event(RI_MOUSE_LEFT_BUTTON_UP);read_frame(b,u);T(!(b[0x39]&0x80)&&(b[0x1d]&0x80),"fire release or aim lost");
 puts("PASS independent one-second fire/charge and release during held weapon aim");
 ready(b,u,0);hm_test_raw_button_event(RI_MOUSE_LEFT_BUTTON_DOWN|RI_MOUSE_LEFT_BUTTON_UP);
 hm_test_legacy_event(WM_LBUTTONDOWN,0,0);hm_test_legacy_event(WM_LBUTTONUP,0,0);
 read_frame(b,other);T(!(b[0x39]&0x80),"other input object received fire");
 read_frame(b,u);T(b[0x39]&0x80,"other callback consumed active-player quick fire");
 read_frame(b,u);T(!(b[0x39]&0x80),"duplicate quick click fired twice");
 puts("PASS other controller callbacks cannot steal fire; delayed legacy duplicate cannot repeat it");
 ready(b,u,0);hm_test_raw_motion(4,0);read_frame(b,u);ax=0;mask=0x8021;
 hm_test_apply_native(u,&ax,&mask);
 T((signed char)ax==16&&(signed char)(ax>>8)==-126&&mask==0x8020,"forward+look conversion");
 puts("PASS keyboard forward plus mouse yaw preserves action bits and full-speed human walking direction");
 ready(b,u,0);hm_test_raw_motion(4,0);read_frame(b,u);ax=0x13572468;mask=0;
 hm_test_apply_native(u,&ax,&mask);T(ax==0x13572468,"existing native joystick overwritten");
 puts("PASS existing joystick input keeps ownership");
 ready(b,u,0);hm_test_raw_motion(4,0);read_frame(b,u);hm_test_set_native_kind(2);ax=mask=0;
 hm_test_apply_native(u,&ax,&mask);T(!ax&&!hm_test_native_ready(),"type2 accepted human packet policy");
 puts("PASS controller-type transition rejects the human-specific walking convention");

 ready(b,u,0);hm_test_raw_motion(4,0);read_frame(b,u);hm_test_set_native_focus(0);hm_test_poll_state();ax=mask=0;
 hm_test_apply_native(u,&ax,&mask);T(!ax&&!hm_test_native_ready(),"background native movement escaped");
 puts("PASS native macOS foreground loss cancels staged body motion");
 ready(b,u,0);hm_test_raw_motion(4,0);read_frame(b,u);hm_test_set_now(1200);ax=mask=0;
 hm_test_apply_native(u,&ax,&mask);T(!ax&&!hm_test_native_ready(),"stale staged motion escaped");
 puts("PASS stale native packet expires without replay");
 ready(b,u,0);hm_test_raw_motion(4,0);read_frame(b,u);hm_test_set_native_context(u,6,7,4);ax=mask=0;
 hm_test_apply_native(u,&ax,&mask);T(!ax&&!hm_test_native_ready(),"weapon transition accepted old body packet");
 ready(b,u,0);hm_test_raw_motion(4,0);read_frame(b,u);hm_test_set_native_context(u,6,0,18);ax=mask=0;
 hm_test_apply_native(u,&ax,&mask);T(!ax&&!hm_test_native_ready(),"camera mode transition accepted old body packet");
 puts("PASS weapon and camera-mode transitions invalidate staged body packets");

 ready(b,u,0);hm_test_raw_button_event(RI_MOUSE_LEFT_BUTTON_DOWN);read_frame(b,u);
 hm_test_set_native_context(u,8,0,7);read_frame(b,u);T(!(b[0x39]&0x80)&&!hm_test_native_ready(),"non-human-turn fire escaped");
 hm_test_raw_button_event(RI_MOUSE_LEFT_BUTTON_UP);
 hm_test_set_native_context(u,6,0,4);read_frame(b,u);hm_test_poll_state();read_frame(b,u);
 T(!(b[0x39]&0x80),"turn change carried fire into new turn");
 puts("PASS turn transition clears fire and motion; no carry into next human turn");
 return 0;
}
