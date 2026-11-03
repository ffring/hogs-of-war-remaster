#include <windows.h>
#include <stdio.h>
#include <string.h>

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
void hm_test_set_native_kind(int);

static void ready(BYTE *b,BYTE *u,int weapon) {
    hm_test_reset_state();hm_test_set_native_context(u,6,weapon,4);read_frame(b,u);
}
#define T(c,m) do { if(!(c)) return fail(m); } while(0)
int main(void) {
 BYTE b[256],u[0x128],other[0x128];memset(u,0,sizeof u);memset(other,0,sizeof other);set_bindings(u);set_bindings(other);
 u[0x108]=0xc8;u[0x10a]=0xd0;u[0x10c]=0xcb;
 hm_test_set_prior(mock_prior_stub);expected_buf=b;expected_user=u;
 T(invoke_checked(b,u)&&prior_args_ok&&prior_calls==1&&prior_return==0x5a,"saved prior ABI");
 puts("PASS saved HogsFix ABI and EDI/result preserved");
 DWORD ax,mask;
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
