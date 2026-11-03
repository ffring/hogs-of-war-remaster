#import <AppKit/AppKit.h>
#include "hogs_mac_focus.h"
#include "focus_mailbox.h"
#include "focus_runloop_wait.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int parse_pid(const char *text, pid_t *out)
{
    char *end = NULL;
    long value;
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno || end == text || *end || value <= 1 || value > INT_MAX) return 0;
    *out = (pid_t)value;
    return 1;
}

static int parse_token(const char *text, uint32_t *lo, uint32_t *hi)
{
    uint64_t value = 0;
    size_t i;
    if (strlen(text) != 16) return 0;
    for (i = 0; i < 16; ++i) {
        unsigned digit;
        char c = text[i];
        if (c >= '0' && c <= '9') digit = (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') digit = (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') digit = (unsigned)(c - 'A' + 10);
        else return 0;
        value = (value << 4) | digit;
    }
    if (!value) return 0;
    *lo = (uint32_t)value;
    *hi = (uint32_t)(value >> 32);
    return 1;
}

static int process_is_alive(pid_t pid)
{
    if (kill(pid, 0) == 0) return 1;
    return errno == EPERM;
}

static int validate_mapping(int fd, HMMacFocusRecord *record,
                            pid_t target, uint32_t token_lo, uint32_t token_hi,
                            HMMacFocusRecord **mapped)
{
    struct stat st;
    HMMacFocusRecord *view;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) ||
        st.st_size != (off_t)sizeof(*record) || st.st_uid != getuid()) return 0;
    view = mmap(NULL, sizeof(*view), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (view == MAP_FAILED) return 0;
    memcpy(record, view, sizeof(*record));
    if (record->magic != HM_MAC_FOCUS_MAGIC ||
        record->version != HM_MAC_FOCUS_VERSION ||
        record->native_game_pid != (uint32_t)target ||
        record->token_lo != token_lo || record->token_hi != token_hi ||
        record->reserved != 0 || (record->seq & 1u)) {
        munmap(view, sizeof(*view));
        return 0;
    }
    *mapped = view;
    return 1;
}

static void publish(HMMacFocusRecord *view, uint32_t *sequence,
                    pid_t target, uint32_t token_lo, uint32_t token_hi,
                    uint32_t active)
{
    uint32_t even = (*sequence + 2u) & ~1u;
    uint32_t odd = even - 1u;
    if (!even) { even = 2u; odd = 1u; }
    __atomic_store_n(&view->seq, odd, __ATOMIC_RELEASE);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    view->magic = HM_MAC_FOCUS_MAGIC;
    view->version = HM_MAC_FOCUS_VERSION;
    view->native_game_pid = (uint32_t)target;
    view->token_lo = token_lo;
    view->token_hi = token_hi;
    view->active = active ? 1u : 0u;
    view->reserved = 0;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    __atomic_store_n(&view->seq, even, __ATOMIC_RELEASE);
    *sequence = even;
}

static void sleep_50ms(void)
{
    /* The timer in this helper keeps an otherwise empty mode alive. */
    hogs_focus_runloop_wait_50ms();
}

int main(int argc, char **argv)
{
    pid_t target;
    uint32_t token_lo, token_hi, sequence;
    int fd;
    HMMacFocusRecord initial;
    HMMacFocusRecord *view = NULL;
    int selftest = 0;

#ifdef HOGS_FOCUS_PUBLISHER_SELFTEST
    if (argc == 5 && strcmp(argv[4], "--selftest") == 0) selftest = 1;
#endif
    if ((!selftest && argc != 4) || (selftest && argc != 5) ||
        !parse_pid(argv[2], &target) || !parse_token(argv[3], &token_lo, &token_hi)) {
        fprintf(stderr, "usage: %s mailbox-path target-pid 16-hex-token%s\n",
                argv[0],
#ifdef HOGS_FOCUS_PUBLISHER_SELFTEST
                " [--selftest]"
#else
                ""
#endif
        );
        return 2;
    }
    if (getppid() != target || !process_is_alive(target)) {
        fprintf(stderr, "target must be the live parent process\n");
        return 3;
    }
    fd = open(argv[1], O_RDWR | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) { perror("open mailbox"); return 4; }
    if (!validate_mapping(fd, &initial, target, token_lo, token_hi, &view)) {
        fprintf(stderr, "mailbox validation failed\n"); close(fd); return 5;
    }
    sequence = initial.seq;

#ifdef HOGS_FOCUS_PUBLISHER_SELFTEST
    if (selftest) {
        publish(view, &sequence, target, token_lo, token_hi, 1);
        sleep_50ms();
        publish(view, &sequence, target, token_lo, token_hi, 0);
        munmap(view, sizeof(*view)); close(fd);
        puts("selftest protocol published two states; no app input read");
        return 0;
    }
#endif

    uint32_t last_active = UINT32_MAX;
    pid_t last_frontmost = -2;
    while (getppid() == target && process_is_alive(target)) {
        @autoreleasepool {
            pid_t frontmost = hogs_mac_frontmost_pid();
            uint32_t active = frontmost == target && frontmost > 0;
            publish(view, &sequence, target, token_lo, token_hi, active);
            if (active != last_active || frontmost != last_frontmost) {
                fprintf(stderr, "focus-state active=%u target_pid=%d front_pid=%d\n",
                        active, (int)target, (int)frontmost);
                fflush(stderr);
                last_active = active;
                last_frontmost = frontmost;
            }
            sleep_50ms();
        }
    }
    publish(view, &sequence, target, token_lo, token_hi, 0);
    munmap(view, sizeof(*view));
    close(fd);
    return 0;
}
