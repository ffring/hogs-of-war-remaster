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

int main(void) {
    BYTE buf[256], user[0x128]; memset(buf, 0, sizeof(buf)); memset(user, 0, sizeof(user)); set_bindings(user);
    hm_test_set_prior(mock_prior_stub); expected_buf=buf; expected_user=user;
    if (!invoke_checked(buf,user) || !prior_args_ok || prior_calls != 1 || prior_return != 0x5a)
        return fail("prior HogsFix stub ABI/EDI/result");
    puts("PASS prior stub ABI called once: stack buffer, EDI player, EDI/result preserved");

    /* Default controlled aim: L1 stays held through stationary reads while
       RMB is down; each read consumes its frame motion, and UP clears it. */
    hm_test_reset_state(); hm_test_set_require_rmb(1); hm_test_set_buttons(0);
    hm_test_raw_button_event(RI_MOUSE_RIGHT_BUTTON_DOWN);
    hm_test_raw_motion(4,0);
    if (read_frame(buf,user) == 0x5a || !hasaim(buf)) return fail("RMB frame delta did not add RIGHT+L1");
    if (read_frame(buf,user) == 0x5a || (buf[0x1d]&0x80)==0 || (buf[0x20]&0x80))
        return fail("stationary RMB read did not hold only L1");
    hm_test_raw_button_event(RI_MOUSE_RIGHT_BUTTON_UP);
    read_frame(buf,user);
    if ((buf[0x1d]&0x80) || (buf[0x20]&0x80))
        return fail("RMB UP did not immediately clear L1 and arrows");
    puts("PASS default controlled aim: stationary RMB holds L1; release clears L1 and aim");

    /* Subthreshold motion only retains a signed fraction; reversal discards
       the prior fraction. There is no direction pulse when the mouse is idle. */
    hm_test_reset_state(); hm_test_raw_button_event(RI_MOUSE_RIGHT_BUTTON_DOWN);
    hm_test_raw_motion(1,0); read_frame(buf,user);
    if ((buf[0x1d]&0x80)==0 || (buf[0x20]&0x80)) return fail("subthreshold motion generated a key");
    hm_test_set_now(1010); hm_test_raw_motion(-1,0); read_frame(buf,user);
    hm_test_set_now(1020); hm_test_raw_motion(1,0); read_frame(buf,user);
    if ((buf[0x20]&0x80) || (buf[0x1d]&0x80)==0) return fail("tiny alternating deltas emitted aim key");
    puts("PASS tiny alternating Raw deltas do not create a direction pulse");

    /* Large frames yield one pulse only. A fresh reversal never inherits a
       queued direction from earlier fast motion. */
    hm_test_reset_state(); hm_test_raw_button_event(RI_MOUSE_RIGHT_BUTTON_DOWN);
    hm_test_raw_motion(7,0);
    if (read_frame(buf,user) == 0x5a || !hasaim(buf)) return fail("fast frame missing single right pulse");
    hm_test_set_now(1010); hm_test_raw_motion(-1,0); read_frame(buf,user);
    if ((buf[0x20]&0x80) || (buf[0x1d]&0x80)==0) return fail("reversal inherited prior direction debt");
    hm_test_set_now(1020); read_frame(buf,user);
    if ((buf[0x20]&0x80) || (buf[0x1d]&0x80)==0) return fail("idle read continued aim direction");
    puts("PASS fast-to-slow reversal and idle do not replay directional backlog");

    /* A stale INI opt-out is normalized by the same policy used by
       load_config, so old free-aim settings cannot inject while idle. */
    hm_test_reset_state(); hm_test_set_require_rmb(0); hm_test_raw_motion(4,0);
    if (read_frame(buf,user) != 0x5a || has_direction(buf) || (buf[0x1d]&0x80))
        return fail("legacy RequireRightButton=0 bypassed the RMB gate");
    puts("PASS legacy RequireRightButton=0 cannot enable idle free aim");

    /* Raw LMB remains authoritative despite async polling zero, including a
       stationary one-second charge, and an actual UP ends it. */
    hm_test_reset_state(); hm_test_set_buttons(0); hm_test_set_cursor(100,100,1); read_frame(buf,user);
    hm_test_set_now(1050); hm_test_raw_button_event(RI_MOUSE_LEFT_BUTTON_DOWN);
    if (!(hm_test_event_buttons_held() & 1) || !(hm_test_event_buttons_known() & 1)) return fail("Raw LMB down recorded");
    for (DWORD t=1050; t<=2050; t+=50) {
        hm_test_set_now(t); hm_test_set_buttons(0);
        read_frame(buf,user);
        if (!(buf[0x39] & 0x80) || !(hm_test_event_buttons_held() & 1)) return fail("Raw held fire overwritten by async zero");
    }
    hm_test_raw_button_event(RI_MOUSE_LEFT_BUTTON_UP); hm_test_set_now(2100);
    if (read_frame(buf,user), (buf[0x39] & 0x80) || (hm_test_event_buttons_held() & 1)) return fail("Raw LMB UP did not end charge");
    puts("PASS Raw button authority holds fire for one second despite async-zero samples; UP releases");

    /* A Raw quick click survives both edges before a game callback. */
    hm_test_reset_state(); hm_test_set_buttons(0); hm_test_set_cursor(10,10,1); read_frame(buf,user);
    hm_test_raw_button_event(RI_MOUSE_LEFT_BUTTON_DOWN);
    hm_test_legacy_event(WM_LBUTTONDOWN, 0, 0); /* duplicate notification */
    hm_test_raw_button_event(RI_MOUSE_LEFT_BUTTON_UP);
    hm_test_legacy_event(WM_LBUTTONUP, 0, 0); /* duplicate notification */
    if (read_frame(buf,user), !(buf[0x39] & 0x80)) return fail("quick Raw click pulse lost");
    if (read_frame(buf,user), (buf[0x39] & 0x80)) return fail("quick Raw click repeated");
    puts("PASS Raw down/up between callbacks emits exactly one fire pulse");

    /* Legacy LMB event state wins over an async sample of zero too. */
    hm_test_reset_state(); hm_test_set_buttons(0); hm_test_set_cursor(10,10,1); read_frame(buf,user);
    hm_test_legacy_event(WM_LBUTTONDOWN, 0, 0);
    read_frame(buf,user);
    if (!(buf[0x39] & 0x80) || !(hm_test_event_buttons_held() & 1)) return fail("legacy LMB down overwritten by async zero");
    puts("PASS legacy LMB down stays held with async state zero");

    /* Reset while a known button is held requires an actual UP before rearm. */
    hm_test_reset_state(); hm_test_raw_button_event(RI_MOUSE_RIGHT_BUTTON_DOWN);
    MSG m; memset(&m,0,sizeof(m)); m.hwnd=(HWND)1; m.message=WM_KILLFOCUS; hm_test_queue_message(&m);
    if (hm_test_is_armed() || !(hm_test_event_buttons_wait_release() & 2)) return fail("focus loss did not preserve required physical release");
    m.message=WM_SETFOCUS; hm_test_queue_message(&m);
    hm_test_set_buttons(0);
    if (hm_test_rearm_ready()) return fail("async zero prematurely satisfied event-held release");
    hm_test_raw_button_event(RI_MOUSE_RIGHT_BUTTON_UP);
    if (!hm_test_is_armed() || !hm_test_rearm_ready()) return fail("actual right UP did not rearm");
    puts("PASS focus loss clears state and waits for actual UP, not async-zero polling");

    /* Raw owns movement while recent: sampled screen delta is discarded and
       cannot replay when Raw ages out. */
    hm_test_reset_state(); hm_test_set_buttons(0); hm_test_set_cursor(200,200,1); read_frame(buf,user);
    hm_test_raw_button_event(RI_MOUSE_RIGHT_BUTTON_DOWN);
    hm_test_raw_motion(4,0); hm_test_set_cursor(212,200,1);
    if (read_frame(buf,user) == 0x5a || !hasaim(buf) || hm_test_poll_dx()!=0) return fail("fresh Raw movement or poll-delta suppression");
    for (DWORD t=1050; t<=1300; t+=50) { hm_test_set_now(t); hm_test_set_cursor(212,200,1); read_frame(buf,user); }
    if (has_direction(buf) || (buf[0x1d]&0x80)==0 || hm_test_mouse_dx()!=0) return fail("stale unselected motion replayed after Raw expiry or RMB L1 dropped");
    puts("PASS one movement source only; poll movement is discarded and held RMB keeps L1 after Raw expires");

    /* UI transition clears held/pending mouse state and injects no mouse keys. */
    hm_test_reset_state(); hm_test_raw_button_event(RI_MOUSE_LEFT_BUTTON_DOWN);
    hm_test_set_ui(1); read_frame(buf,user);
    if ((buf[0x39]&0x80) || hm_test_event_buttons_held() || !hm_test_event_buttons_wait_release())
        return fail("UI gate did not clear and disarm held mouse state");
    puts("PASS UI transition clears mouse state and remains disarmed until release");

    /* Native macOS focus mailbox is an independent gate: a Wine HWND can
       still report itself foreground while another macOS app owns focus. */
    hm_test_reset_state(); hm_test_raw_button_event(RI_MOUSE_LEFT_BUTTON_DOWN);
    hm_test_set_native_focus(0);
    memset(&m, 0, sizeof(m)); m.hwnd=(HWND)1; m.message=WM_MOUSEMOVE;
    hm_test_queue_message(&m); /* exercises the same foreground-loss poll */
    if (hm_test_is_armed() || hm_test_event_buttons_held() || !hm_test_event_buttons_wait_release())
        return fail("native macOS focus loss did not clear/disarm held input");
    memset(buf, 0, sizeof(buf)); expected_buf=buf; expected_user=user;
    if (hm_test_run_callback(buf,user) != 0x5a || (buf[0x39]&0x80) || hasaim(buf))
        return fail("native macOS background state injected input");
    hm_test_set_native_focus(1); hm_test_queue_message(&m);
    if (hm_test_is_armed()) return fail("native focus regain rearmed without release");
    hm_test_raw_button_event(RI_MOUSE_LEFT_BUTTON_UP);
    hm_test_queue_message(&m);
    puts("PASS native macOS focus mailbox blocks aim/fire, clears held state, and requires release to rearm");

    /* Losing a previously verified Raw Input registration is not the same as
       ordinary motion silence. It must clear event-held fire and disarm, while
       preserving the prior HogsFix callback result and refusing async-zero as
       proof of UP. */
    hm_test_reset_state(); hm_test_raw_button_event(RI_MOUSE_LEFT_BUTTON_DOWN);
    hm_test_set_raw_registration_match(0); hm_test_poll_state();
    if (hm_test_is_armed() || hm_test_event_buttons_held() || !(hm_test_event_buttons_wait_release() & 1))
        return fail("Raw registration loss did not clear and disarm event-held fire");
    hm_test_set_buttons(0); memset(buf, 0, sizeof(buf)); expected_buf=buf; expected_user=user;
    if (hm_test_run_callback(buf,user) != 0x5a || (buf[0x39]&0x80) || hm_test_event_buttons_held())
        return fail("Raw registration loss retained Cross or overwrote prior callback result");
    if (hm_test_is_armed() || !(hm_test_event_buttons_wait_release() & 1))
        return fail("async-zero poll rearmed after Raw registration loss");
    puts("PASS verified Raw registration loss clears held fire, preserves prior result, and requires a trusted release");
    return 0;
}
