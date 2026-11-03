#ifndef HOGS_MAC_FOCUS_GATE_H
#define HOGS_MAC_FOCUS_GATE_H

/* Game-local native foreground mailbox. No event monitoring or key logging.
   Protocol: eight little-endian uint32 words, seq odd during native writes. */
#ifdef HOGSMOUSE_TEST
static int hm_mac_focus_gate(void) { return g_test_native_focus != 0; }
static int hm_mac_focus_status_code(void) { return g_test_native_focus ? 1 : 2; }
#else
#include <windows.h>
#include <stdint.h>

enum { HM_MAC_FOCUS_MAGIC = 0x484d4631u, HM_MAC_FOCUS_VERSION = 1u };
typedef struct {
    uint32_t magic, version, seq, native_game_pid;
    uint32_t token_lo, token_hi, active, reserved;
} HMMacFocusRecord;
typedef char HMMacFocusRecordMustBe32Bytes[(sizeof(HMMacFocusRecord)==32)?1:-1];

static const volatile HMMacFocusRecord *hm_mf_view;
static int hm_mf_initialized, hm_mf_required, hm_mf_valid_config;
static int hm_mf_status, hm_mf_seen_sequence, hm_mf_verified_heartbeat;
static uint32_t hm_mf_token_lo, hm_mf_token_hi, hm_mf_seq;
static DWORD hm_mf_last_change, hm_mf_next_open;
static char hm_mf_path[MAX_PATH];

static void hm_mac_focus_init(void) {
    char flag[8]={0}, token[18]={0};
    uint64_t value=0;
    DWORD n;
    unsigned i;
    if (hm_mf_initialized) return;
    hm_mf_initialized=1;
    n=GetEnvironmentVariableA("HOGS_MAC_FOCUS_REQUIRED",flag,sizeof(flag));
    if (!n || (n==1 && flag[0]=='0')) return; /* Ordinary Windows launch. */
    hm_mf_required=1;
    hm_mf_status=3;
    if (n!=1 || flag[0]!='1') return;
    n=GetEnvironmentVariableA("HOGS_MAC_FOCUS_FILE",hm_mf_path,sizeof(hm_mf_path));
    if (!n || n>=sizeof(hm_mf_path)) return;
    n=GetEnvironmentVariableA("HOGS_MAC_FOCUS_TOKEN",token,sizeof(token));
    if (n!=16) return;
    for (i=0;i<16;i++) {
        unsigned x;
        char c=token[i];
        if (c>='0' && c<='9') x=(unsigned)(c-'0');
        else if (c>='a' && c<='f') x=(unsigned)(c-'a'+10);
        else if (c>='A' && c<='F') x=(unsigned)(c-'A'+10);
        else return;
        value=(value<<4)|x;
    }
    if (!value) return;
    hm_mf_token_lo=(uint32_t)value;
    hm_mf_token_hi=(uint32_t)(value>>32);
    hm_mf_valid_config=1;
}

static int hm_mac_focus_open(DWORD now) {
    HANDLE file, mapping;
    LARGE_INTEGER size;
    if (hm_mf_view) return 1;
    if (hm_mf_next_open && (LONG)(now-hm_mf_next_open)<0) return 0;
    hm_mf_next_open=now+1000;
    file=CreateFileA(hm_mf_path,GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,NULL);
    if (file==INVALID_HANDLE_VALUE) { hm_mf_status=4; return 0; }
    if (!GetFileSizeEx(file,&size) || size.QuadPart!=sizeof(HMMacFocusRecord)) {
        CloseHandle(file); hm_mf_status=5; return 0;
    }
    mapping=CreateFileMappingA(file,NULL,PAGE_READONLY,0,sizeof(HMMacFocusRecord),NULL);
    CloseHandle(file);
    if (!mapping) { hm_mf_status=4; return 0; }
    hm_mf_view=(const volatile HMMacFocusRecord *)MapViewOfFile(mapping,FILE_MAP_READ,0,0,sizeof(HMMacFocusRecord));
    CloseHandle(mapping);
    if (!hm_mf_view) { hm_mf_status=4; return 0; }
    return 1;
}

static int hm_mac_focus_gate(void) {
    HMMacFocusRecord record;
    uint32_t before, after;
    DWORD now;
    hm_mac_focus_init();
    if (!hm_mf_required) { hm_mf_status=0; return 1; }
    if (!hm_mf_valid_config) { hm_mf_status=3; return 0; }
    now=GetTickCount();
    if (!hm_mac_focus_open(now)) return 0;
    before=hm_mf_view->seq;
    MemoryBarrier();
    record.magic=hm_mf_view->magic; record.version=hm_mf_view->version;
    record.native_game_pid=hm_mf_view->native_game_pid;
    record.token_lo=hm_mf_view->token_lo; record.token_hi=hm_mf_view->token_hi;
    record.active=hm_mf_view->active; record.reserved=hm_mf_view->reserved;
    MemoryBarrier();
    after=hm_mf_view->seq;
    if (!before || (before&1) || before!=after || record.magic!=HM_MAC_FOCUS_MAGIC ||
        record.version!=HM_MAC_FOCUS_VERSION || !record.native_game_pid ||
        record.token_lo!=hm_mf_token_lo || record.token_hi!=hm_mf_token_hi || record.active>1 || record.reserved) {
        hm_mf_status=5; return 0;
    }
    if (!hm_mf_seen_sequence) {
        hm_mf_seq=before; hm_mf_seen_sequence=1; hm_mf_last_change=now;
        hm_mf_status=7; return 0; /* Require a live publisher, not a stale file. */
    }
    if (before!=hm_mf_seq) {
        hm_mf_seq=before; hm_mf_last_change=now; hm_mf_verified_heartbeat=1;
    }
    if ((DWORD)(now-hm_mf_last_change)>1000) {
        hm_mf_verified_heartbeat=0; hm_mf_status=6; return 0;
    }
    if (!hm_mf_verified_heartbeat) { hm_mf_status=7; return 0; }
    hm_mf_status=record.active?1:2;
    return record.active!=0;
}
static int hm_mac_focus_status_code(void) { return hm_mf_status; }
#endif
#endif
