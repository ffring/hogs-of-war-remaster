#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <objbase.h>
#include <dplay.h>
#include <dplobby.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A private test application GUID. This never impersonates Hogs of War. */
static const GUID probe_app_guid =
    {0x732f9b46, 0x718c, 0x4eaf, {0x9b, 0x42, 0x48, 0x57, 0x50, 0x52, 0x42, 0x31}};
static const GUID null_guid = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};

#define PROBE_MAGIC 0x31504448u /* "HDP1" */
#define MSG_HELLO  1u
#define MSG_PING   2u
#define MSG_ACK    3u
#define MSG_CONFIRM 4u
#define RECEIVE_CAP 512u

typedef struct probe_message {
    DWORD magic;
    DWORD type;
    DWORD nonce;
    DWORD reserved;
} probe_message;

static LPDIRECTPLAY4A g_dp;
static LPDIRECTPLAYLOBBY3A g_lobby;
static BOOL g_com_initialized;
static const char *g_fail_stage = "startup";
static HRESULT g_fail_hr = S_OK;
static DWORD g_timeout_seconds = 30;
static ULONGLONG g_process_start;
static volatile const char *g_active_step = "arguments";
static HANDLE g_watchdog_done;
static HANDLE g_watchdog_thread;
static BOOL g_failure_reported;

static ULONGLONG elapsed_ms(void) { return GetTickCount64() - g_process_start; }

static void trace_line(const char *kind,const char *stage,HRESULT hr,BOOL with_hr)
{
    if (with_hr)
        printf("{\"event\":\"%s\",\"stage\":\"%s\",\"elapsed_ms\":%llu,\"hr\":\"0x%08lX\"}\n",
               kind,stage,(unsigned long long)elapsed_ms(),(unsigned long)hr);
    else
        printf("{\"event\":\"%s\",\"stage\":\"%s\",\"elapsed_ms\":%llu}\n",
               kind,stage,(unsigned long long)elapsed_ms());
    fflush(stdout);
}

static void trace_begin(const char *stage)
{
    InterlockedExchangePointer((PVOID volatile *)&g_active_step,(PVOID)stage);
    trace_line("begin",stage,S_OK,FALSE);
}

static void trace_end(const char *stage,HRESULT hr)
{
    InterlockedExchangePointer((PVOID volatile *)&g_active_step,(PVOID)"between_steps");
    trace_line("end",stage,hr,TRUE);
}

static void trace_end_value(const char *stage,DWORD value)
{
    InterlockedExchangePointer((PVOID volatile *)&g_active_step,(PVOID)"between_steps");
    printf("{\"event\":\"end\",\"stage\":\"%s\",\"elapsed_ms\":%llu,\"return_value\":%lu}\n",
           stage,(unsigned long long)elapsed_ms(),(unsigned long)value);
    fflush(stdout);
}

static size_t append_text(char *dst,size_t at,size_t cap,const char *src)
{
    while (*src && at+1<cap) dst[at++]=*src++;
    if (cap) dst[at<cap?at:cap-1]='\0';
    return at;
}

static size_t append_uint(char *dst,size_t at,size_t cap,ULONGLONG value)
{
    char digits[24];size_t n=0;
    do { digits[n++]=(char)('0'+value%10);value/=10; } while(value && n<sizeof(digits));
    while(n && at+1<cap) dst[at++]=digits[--n];
    if(cap)dst[at<cap?at:cap-1]='\0';
    return at;
}

static DWORD WINAPI process_watchdog(void *unused)
{
    DWORD wait_ms=g_timeout_seconds*1000u;
    (void)unused;
    if (WaitForSingleObject(g_watchdog_done,wait_ms)!=WAIT_TIMEOUT) return 0;
    {
        char line[512]; DWORD written=0;size_t at=0;
        const char *step=(const char *)InterlockedCompareExchangePointer(
            (PVOID volatile *)&g_active_step,NULL,NULL);
        at=append_text(line,at,sizeof(line),"{\"ok\":false,\"stage\":\"process_timeout\",\"active_step\":\"");
        at=append_text(line,at,sizeof(line),step ? step : "unknown");
        at=append_text(line,at,sizeof(line),"\",\"elapsed_ms\":");
        at=append_uint(line,at,sizeof(line),elapsed_ms());
        at=append_text(line,at,sizeof(line),"}\r\n");
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),line,(DWORD)at,&written,NULL);
    }
    TerminateProcess(GetCurrentProcess(),124);
    return 0;
}

static int start_watchdog(void)
{
    g_process_start=GetTickCount64();
    g_watchdog_done=CreateEventA(NULL,TRUE,FALSE,NULL);
    if (!g_watchdog_done) return 0;
    g_watchdog_thread=CreateThread(NULL,0,process_watchdog,NULL,0,NULL);
    if (!g_watchdog_thread) { CloseHandle(g_watchdog_done);g_watchdog_done=NULL;return 0; }
    trace_begin("process_started");
    return 1;
}

static void stop_watchdog(void)
{
    if (g_watchdog_done) SetEvent(g_watchdog_done);
    if (g_watchdog_thread) { WaitForSingleObject(g_watchdog_thread,INFINITE);CloseHandle(g_watchdog_thread);g_watchdog_thread=NULL; }
    if (g_watchdog_done) { CloseHandle(g_watchdog_done);g_watchdog_done=NULL; }
}

#define TRACE_HR(stage, expression, result) do { \
    trace_begin(stage); (result)=(expression); trace_end(stage,(result)); \
} while (0)

static void fail_at(const char *stage, HRESULT hr)
{
    g_fail_stage = stage;
    g_fail_hr = hr;
    if (!g_failure_reported) {
        printf("{\"ok\":false,\"stage\":\"%s\",\"hr\":\"0x%08lX\"}\n",
               stage,(unsigned long)hr);
        fflush(stdout);g_failure_reported=TRUE;
    }
}

static int parse_timeout(const char *s, DWORD *out)
{
    char *end = NULL;
    unsigned long n;
    if (!s || !*s) return 0;
    n = strtoul(s, &end, 10);
    if (*end || n < 1 || n > 3600) return 0;
    *out = (DWORD)n;
    return 1;
}

static int valid_host(const char *host)
{
    size_t i,n,label=0;
    if (!host || !(n=strlen(host)) || n>253 || host[0]=='.' || host[n-1]=='.' ||
        host[0]=='-' || host[n-1]=='-') return 0;
    for (i=0;i<n;++i) {
        char c=host[i];
        if (!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='-')) return 0;
        if (c=='.') {
            if (!label || label>63 || host[i-1]=='-') return 0;
            label=0;
        } else {
            if (!label && c=='-') return 0;
            if (++label>63) return 0;
        }
    }
    return label>0 && label<=63 && host[n-1]!='-';
}

static void print_help(void)
{
    puts("Usage: dplay-probe.exe --host [--timeout SECONDS]");
    puts("       dplay-probe.exe --join HOST_OR_IP [--timeout SECONDS]");
    puts("Uses a private DirectPlay4A test session over TCP/IP; it does not launch Hogs of War.");
    puts("--timeout bounds the whole process, including DirectPlay calls and cleanup (1..3600 seconds).");
    puts("Join hosts may contain only ASCII letters, digits, dots, and hyphens.");
}

static HRESULT make_tcp_address(const char *host, BYTE **out, DWORD *out_size)
{
    DPCOMPOUNDADDRESSELEMENT elements[2];
    DWORD size = 0;
    HRESULT hr;

    elements[0].guidDataType = DPAID_ServiceProvider;
    elements[0].dwDataSize = sizeof(GUID);
    elements[0].lpData = (LPVOID)&DPSPGUID_TCPIP;
    elements[1].guidDataType = DPAID_INet;
    elements[1].dwDataSize = (DWORD)strlen(host) + 1;
    elements[1].lpData = (LPVOID)host;

    trace_begin("compound_address_size");
    hr = IDirectPlayLobby_CreateCompoundAddress(g_lobby, elements, 2, NULL, &size);
    trace_end("compound_address_size",hr);
    if (hr != DPERR_BUFFERTOOSMALL) return hr;

    *out = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
    if (!*out) return E_OUTOFMEMORY;
    trace_begin("compound_address_build");
    hr = IDirectPlayLobby_CreateCompoundAddress(g_lobby, elements, 2, *out, &size);
    trace_end("compound_address_build",hr);
    if (FAILED(hr)) {
        HeapFree(GetProcessHeap(), 0, *out);
        *out = NULL;
        return hr;
    }
    *out_size = size;
    return S_OK;
}

static HRESULT init_directplay(const char *host)
{
    HRESULT hr;
    BYTE *address = NULL;
    DWORD address_size = 0;

    TRACE_HR("cocreate_directplay",
      CoCreateInstance(&CLSID_DirectPlay, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IDirectPlay4A, (void **)&g_dp),hr);
    if (FAILED(hr)) return hr;

    TRACE_HR("cocreate_lobby",
      CoCreateInstance(&CLSID_DirectPlayLobby, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IDirectPlayLobby3A, (void **)&g_lobby),hr);
    if (FAILED(hr)) return hr;

    hr = make_tcp_address(host, &address, &address_size);
    if (SUCCEEDED(hr)) {
        trace_begin("initialize_connection");
        hr = IDirectPlayX_InitializeConnection(g_dp, address, 0);
        trace_end("initialize_connection",hr);
    }

    if (address) HeapFree(GetProcessHeap(), 0, address);
    return hr;
}

static BOOL CALLBACK enum_session_cb(LPCDPSESSIONDESC2 session,
                                     LPDWORD timeout, DWORD flags, LPVOID ctx)
{
    GUID *instance = (GUID *)ctx;
    if (flags & DPESC_TIMEDOUT) { if (timeout) *timeout=0; return FALSE; }
    if (session && IsEqualGUID(&session->guidApplication, &probe_app_guid)) {
        *instance = session->guidInstance;
        return FALSE;
    }
    return TRUE;
}

static HRESULT create_player(const char *stage,const char *name,DPID *id)
{
    DPNAME player_name;
    ZeroMemory(&player_name, sizeof(player_name));
    player_name.dwSize = sizeof(player_name);
    player_name.lpszShortNameA = (LPSTR)name;
    player_name.lpszLongNameA = (LPSTR)name;
    HRESULT hr;
    TRACE_HR(stage,IDirectPlayX_CreatePlayer(g_dp,id,&player_name,NULL,NULL,0,0),hr);
    return hr;
}

static HRESULT receive_until(DWORD timeout_ms, DPID *from, probe_message *message,
                             BOOL *got_message)
{
    ULONGLONG deadline = GetTickCount64() + timeout_ms;
    *got_message = FALSE;
    for (;;) {
        BYTE buffer[RECEIVE_CAP];
        DWORD size = sizeof(buffer);
        DPID sender = DPID_UNKNOWN, recipient = DPID_UNKNOWN;
        HRESULT hr = IDirectPlayX_Receive(g_dp, &sender, &recipient, 0, buffer, &size);
        if (hr == DP_OK) {
            if (size >= sizeof(probe_message)) {
                probe_message candidate;
                memcpy(&candidate, buffer, sizeof(candidate));
                if (candidate.magic == PROBE_MAGIC) {
                    *from = sender;
                    *message = candidate;
                    *got_message = TRUE;
                    return S_OK;
                }
            }
            continue; /* Ignore DirectPlay system messages. */
        }
        if (hr != DPERR_NOMESSAGES) return hr;
        if (GetTickCount64() >= deadline) return HRESULT_FROM_WIN32(WAIT_TIMEOUT);
        Sleep(10);
    }
}

static DWORD make_nonce(void)
{
    DWORD n = GetTickCount() ^ GetCurrentProcessId() ^ (DWORD)(ULONG_PTR)&probe_app_guid;
    return n ? n : 0x6f726b31u;
}

static int run_host(void)
{
    DPSESSIONDESC2 session;
    DPID local_player = 0, from = DPID_UNKNOWN;
    probe_message incoming;
    BOOL got = FALSE;
    HRESULT hr;
    DWORD max_ms = g_timeout_seconds * 1000u;

    TRACE_HR("com_initialize",CoInitializeEx(NULL, COINIT_MULTITHREADED),hr);
    if (FAILED(hr)) { fail_at("com_init", hr); goto done; }
    g_com_initialized = TRUE;

    hr = init_directplay(""); /* Empty DPAID_INet selects the local host interface. */
    if (FAILED(hr)) { fail_at("tcp_provider_init", hr); goto done; }

    ZeroMemory(&session, sizeof(session));
    session.dwSize = sizeof(session);
    session.guidApplication = probe_app_guid;
    session.dwMaxPlayers = 2;
    session.lpszSessionNameA = "Codex DirectPlay TCP/IP Probe";
    TRACE_HR("host_open",IDirectPlayX_Open(g_dp,&session,DPOPEN_CREATE),hr);
    if (FAILED(hr)) { fail_at("host_open", hr); goto done; }

    hr = create_player("host_create_player","probe-host",&local_player);
    if (FAILED(hr)) { fail_at("host_create_player", hr); goto done; }

    printf("{\"ok\":true,\"stage\":\"ready\",\"role\":\"host\",\"timeout_seconds\":%lu}\n",
           (unsigned long)g_timeout_seconds);
    fflush(stdout);

    {
        ULONGLONG deadline = GetTickCount64() + max_ms;
        BOOL saw_ping = FALSE;
        trace_begin("host_wait_hello");
        for (;;) {
            ULONGLONG now = GetTickCount64();
            DWORD wait_ms;
            if (now >= deadline) {
                fail_at("host_receive_hello", HRESULT_FROM_WIN32(WAIT_TIMEOUT));
                goto done;
            }
            wait_ms = (DWORD)((deadline - now) > 250 ? 250 : deadline - now);
            hr = receive_until(wait_ms, &from, &incoming, &got);
            if (hr == HRESULT_FROM_WIN32(WAIT_TIMEOUT)) continue;
            if (FAILED(hr)) { fail_at("host_receive_hello", hr); goto done; }
            if (got && incoming.type == MSG_PING) saw_ping = TRUE;
            if (got && incoming.type == MSG_HELLO) break;
        }
        trace_end("host_wait_hello",S_OK);

        {
            probe_message ack = {PROBE_MAGIC, MSG_ACK, incoming.nonce, 0};
            TRACE_HR("host_send_ack",IDirectPlayX_Send(g_dp,local_player,DPID_ALLPLAYERS,
                                   DPSEND_GUARANTEED,&ack,sizeof(ack)),hr);
            if (FAILED(hr)) { fail_at("host_send_ack", hr); goto done; }
        }

        /* Require a client confirmation so exit 0 means a real two-way exchange. */
        trace_begin("host_wait_confirm");
        for (;;) {
            ULONGLONG now = GetTickCount64();
            DWORD wait_ms;
            probe_message m;
            DPID sender = DPID_UNKNOWN;
            BOOL have = FALSE;
            if (now >= deadline) {
                fail_at("host_receive_confirm", HRESULT_FROM_WIN32(WAIT_TIMEOUT));
                goto done;
            }
            wait_ms = (DWORD)((deadline - now) > 250 ? 250 : deadline - now);
            hr = receive_until(wait_ms, &sender, &m, &have);
            if (hr == HRESULT_FROM_WIN32(WAIT_TIMEOUT)) continue;
            if (FAILED(hr)) { fail_at("host_receive_confirm", hr); goto done; }
            if (have && m.type == MSG_PING && m.nonce == incoming.nonce) saw_ping = TRUE;
            if (have && m.type == MSG_CONFIRM && m.nonce == incoming.nonce) break;
        }
        trace_end("host_wait_confirm",S_OK);
        printf("{\"ok\":true,\"stage\":\"peer_exchange\",\"role\":\"host\",\"reliable_ack_sent\":true,\"unreliable_ping_seen\":%s}\n",
               saw_ping ? "true" : "false");
        fflush(stdout);
    }
    goto done;

done:
    if (g_dp) {
        trace_begin("host_close");hr=IDirectPlayX_Close(g_dp);trace_end("host_close",hr);
        trace_begin("host_release");{ ULONG refs=IDirectPlayX_Release(g_dp);trace_end_value("host_release",refs); }
        g_dp = NULL;
    }
    if (g_lobby) { trace_begin("lobby_release");{ ULONG refs=IDirectPlayLobby_Release(g_lobby);trace_end_value("lobby_release",refs); }g_lobby = NULL; }
    if (g_com_initialized) { trace_begin("com_uninitialize");CoUninitialize();trace_end("com_uninitialize",S_OK); }
    if (FAILED(g_fail_hr)) {
        return 1;
    }
    return 0;
}

static int run_join(const char *host)
{
    DPSESSIONDESC2 query;
    GUID instance;
    DPID local_player = 0, from = DPID_UNKNOWN;
    HRESULT hr;
    BOOL got = FALSE;
    DWORD nonce = make_nonce();
    probe_message incoming;
    ULONGLONG deadline;

    TRACE_HR("com_initialize",CoInitializeEx(NULL, COINIT_MULTITHREADED),hr);
    if (FAILED(hr)) { fail_at("com_init", hr); goto done; }
    g_com_initialized = TRUE;

    hr = init_directplay(host);
    if (FAILED(hr)) { fail_at("tcp_provider_init", hr); goto done; }

    ZeroMemory(&query, sizeof(query));
    query.dwSize = sizeof(query);
    query.guidApplication = probe_app_guid;
    ZeroMemory(&instance, sizeof(instance));
    TRACE_HR("enum_sessions",IDirectPlayX_EnumSessions(g_dp,&query,3000,enum_session_cb,
                                   &instance,DPENUMSESSIONS_AVAILABLE),hr);
    if (FAILED(hr)) { fail_at("enum_sessions", hr); goto done; }
    if (IsEqualGUID(&instance, &null_guid)) {
        fail_at("enum_sessions_no_probe_host", DPERR_NOSESSIONS);
        goto done;
    }

    query.guidInstance = instance;
    TRACE_HR("join_open",IDirectPlayX_Open(g_dp,&query,DPOPEN_JOIN),hr);
    if (FAILED(hr)) { fail_at("join_open", hr); goto done; }

    hr = create_player("join_create_player","probe-client",&local_player);
    if (FAILED(hr)) { fail_at("join_create_player", hr); goto done; }

    {
        probe_message hello = {PROBE_MAGIC, MSG_HELLO, nonce, 0};
        probe_message ping = {PROBE_MAGIC, MSG_PING, nonce, 0};
        TRACE_HR("send_reliable",IDirectPlayX_Send(g_dp,local_player,DPID_ALLPLAYERS,
                               DPSEND_GUARANTEED,&hello,sizeof(hello)),hr);
        if (FAILED(hr)) { fail_at("send_reliable", hr); goto done; }
        TRACE_HR("send_unreliable",IDirectPlayX_Send(g_dp,local_player,DPID_ALLPLAYERS,0,
                               &ping,sizeof(ping)),hr);
        if (FAILED(hr)) { fail_at("send_unreliable", hr); goto done; }
    }

    deadline = GetTickCount64() + (ULONGLONG)g_timeout_seconds * 1000u;
    trace_begin("join_wait_ack");
    while (GetTickCount64() < deadline) {
        ULONGLONG remaining = deadline - GetTickCount64();
        DWORD wait_ms = remaining > 250 ? 250 : (DWORD)remaining;
        hr = receive_until(wait_ms, &from, &incoming, &got);
        if (hr == HRESULT_FROM_WIN32(WAIT_TIMEOUT)) continue;
        if (FAILED(hr)) { fail_at("receive_ack", hr); goto done; }
        if (got && incoming.type == MSG_ACK && incoming.nonce == nonce) {
            probe_message confirm = {PROBE_MAGIC, MSG_CONFIRM, nonce, 0};
            TRACE_HR("send_confirm",IDirectPlayX_Send(g_dp,local_player,DPID_ALLPLAYERS,
                                   DPSEND_GUARANTEED,&confirm,sizeof(confirm)),hr);
            if (FAILED(hr)) { fail_at("send_confirm", hr); goto done; }
            printf("{\"ok\":true,\"stage\":\"peer_exchange\",\"role\":\"join\",\"host\":\"%s\",\"reliable_ack_received\":true}\n", host);
            fflush(stdout);
            trace_end("join_wait_ack",S_OK);
            goto done;
        }
    }
    trace_end("join_wait_ack",HRESULT_FROM_WIN32(WAIT_TIMEOUT));
    fail_at("receive_ack_timeout", HRESULT_FROM_WIN32(WAIT_TIMEOUT));

done:
    if (g_dp) {
        trace_begin("join_close");hr=IDirectPlayX_Close(g_dp);trace_end("join_close",hr);
        trace_begin("join_release");{ ULONG refs=IDirectPlayX_Release(g_dp);trace_end_value("join_release",refs); }
        g_dp = NULL;
    }
    if (g_lobby) { trace_begin("lobby_release");{ ULONG refs=IDirectPlayLobby_Release(g_lobby);trace_end_value("lobby_release",refs); }g_lobby = NULL; }
    if (g_com_initialized) { trace_begin("com_uninitialize");CoUninitialize();trace_end("com_uninitialize",S_OK); }
    if (FAILED(g_fail_hr)) {
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    BOOL host_mode = FALSE;
    const char *join_host = NULL;
    int i,rc;

    setvbuf(stdout,NULL,_IONBF,0);
    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            print_help();
            return 0;
        } else if (!strcmp(argv[i], "--host")) {
            host_mode = TRUE;
        } else if (!strcmp(argv[i], "--join") && i + 1 < argc) {
            join_host = argv[++i];
        } else if (!strcmp(argv[i], "--timeout") && i + 1 < argc) {
            if (!parse_timeout(argv[++i], &g_timeout_seconds)) {
                puts("{\"ok\":false,\"stage\":\"arguments\",\"hr\":\"invalid_timeout\"}");
                return 2;
            }
        } else {
            puts("{\"ok\":false,\"stage\":\"arguments\",\"hr\":\"invalid_argument\"}");
            return 2;
        }
    }

    if ((host_mode && join_host) || (!host_mode && !join_host)) {
        puts("{\"ok\":false,\"stage\":\"arguments\",\"hr\":\"choose_one_role\"}");
        return 2;
    }
    if (join_host && !valid_host(join_host)) {
        puts("{\"ok\":false,\"stage\":\"arguments\",\"hr\":\"invalid_host\"}");
        return 2;
    }
    if (!start_watchdog()) {
        puts("{\"ok\":false,\"stage\":\"watchdog_start\",\"hr\":\"thread_creation_failed\"}");
        return 2;
    }
    printf("{\"event\":\"startup\",\"probe_version\":2,\"pid\":%lu,\"role\":\"%s\",\"timeout_seconds\":%lu}\n",
           (unsigned long)GetCurrentProcessId(),host_mode?"host":"join",(unsigned long)g_timeout_seconds);
    rc=host_mode ? run_host() : run_join(join_host);
    trace_line("end","process",rc ? E_FAIL : S_OK,TRUE);
    stop_watchdog();
    return rc;
}
