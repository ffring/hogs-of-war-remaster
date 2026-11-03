#include <libproc.h>
#include <sys/file.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <spawn.h>
#include <sys/mman.h>
#include "focus_mailbox.h"

extern char **environ;

#ifndef HOGS_ONLINE_APP_PATH
#define HOGS_ONLINE_APP_PATH "/Applications/Hogs of War.app"
#endif

static int exists(const char *path, int mode) { return access(path, mode) == 0; }

static int files_ok(const char *support, const char *game, const char *wine) {
    char path[PATH_MAX];
    const char *required[] = {
        "warhogs_.exe", "HogsFix.asi", "HogsFix.ini", "dinput.dll",
        "ddraw.dll", "d3dimm.dll", "d3d11.dll", "dxgi.dll",
        "dgVoodoo.conf", "dxvk.conf"
    };
    const char *builtin[] = { "dplayx.dll", "dpwsockx.dll", "wsock32.dll", "mswsock.dll" };
    if (!exists(wine, X_OK)) return 0;
    for (size_t i = 0; i < sizeof(required)/sizeof(required[0]); i++) {
        snprintf(path, sizeof(path), "%s/%s", game, required[i]);
        if (!exists(path, R_OK)) return 0;
    }
    for (size_t i = 0; i < sizeof(builtin)/sizeof(builtin[0]); i++) {
        snprintf(path, sizeof(path), "%s/wine/lib/wine/i386-windows/%s", support, builtin[i]);
        if (!exists(path, R_OK)) return 0;
    }
    return 1;
}

static int native_directplay_ready(const char *prefix) {
    char path[PATH_MAX];
    const char *marker = "/drive_c/windows/.hogs-native-directplay-ready";
    const char *dlls[] = { "dplayx.dll", "dpwsockx.dll" };
    snprintf(path, sizeof(path), "%s%s", prefix, marker);
    if (!exists(path, R_OK)) return 0;
    for (size_t i = 0; i < sizeof(dlls)/sizeof(dlls[0]); i++) {
        snprintf(path, sizeof(path), "%s/drive_c/windows/syswow64/%s", prefix, dlls[i]);
        if (!exists(path, R_OK)) return 0;
    }
    return 1;
}

static int process_has_hogs_args(pid_t pid) {
    int mib[3] = { CTL_KERN, KERN_PROCARGS2, pid };
    size_t size = 0;
    if (sysctl(mib, 3, NULL, &size, NULL, 0) != 0 || size < sizeof(int)) return 0;
    char *buf = malloc(size + 1);
    if (!buf) return 0;
    int found = 0;
    if (sysctl(mib, 3, buf, &size, NULL, 0) == 0 && size >= sizeof(int)) {
        int argc = 0;
        memcpy(&argc, buf, sizeof(argc));
        char *p = buf + sizeof(argc), *end = buf + size;
        if (p < end) p += strnlen(p, (size_t)(end - p)) + 1;
        while (p < end && *p == '\0') p++;
        for (int i = 0; i < argc && p < end; i++) {
            size_t len = strnlen(p, (size_t)(end - p));
            if (len && strcasestr(p, "warhogs_.exe")) { found = 1; break; }
            p += len + 1;
        }
    }
    free(buf);
    return found;
}

static int hogs_is_running(void) {
    int total = proc_listallpids(NULL, 0);
    if (total <= 0) return 1;
    int cap = total + 64;
    pid_t *pids = calloc((size_t)cap, sizeof(pid_t));
    if (!pids) return 1;
    int count = proc_listallpids(pids, cap * (int)sizeof(pid_t));
    if (count < 0) { free(pids); return 1; }
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (pids[i] > 0 && pids[i] != getpid() && process_has_hogs_args(pids[i])) {
            found = 1; break;
        }
    }
    free(pids);
    return found;
}

static int start_focus_publisher(const char *contents, const char *game, int game_lock) {
    char publisher[PATH_MAX], mailbox[PATH_MAX], token[17], pid_text[32];
    uint64_t nonce = 0;
    snprintf(publisher, sizeof(publisher), "%s/MacOS/hogs-focus-publisher", contents);
    snprintf(mailbox, sizeof(mailbox), "%s/HogsMouse.focus", game);
    arc4random_buf(&nonce, sizeof(nonce));
    if (!nonce) nonce = 1;
    snprintf(token, sizeof(token), "%016llx", (unsigned long long)nonce);
    snprintf(pid_text, sizeof(pid_text), "%d", getpid());

    setenv("HOGS_MAC_FOCUS_REQUIRED", "1", 1);
    setenv("HOGS_MAC_FOCUS_FILE", "C:\\HogsofWar\\HogsMouse.focus", 1);
    setenv("HOGS_MAC_FOCUS_TOKEN", token, 1);

    int fd = open(mailbox, O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) return 0;
    struct stat st;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_uid != getuid() ||
        fchmod(fd, 0600) || ftruncate(fd, 32)) {
        close(fd);
        return 0;
    }
    HMMacFocusRecord *record = mmap(NULL, 32, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (record == MAP_FAILED) { close(fd); return 0; }
    record->seq = 1;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    record->magic = HM_MAC_FOCUS_MAGIC;
    record->version = HM_MAC_FOCUS_VERSION;
    record->native_game_pid = (uint32_t)getpid();
    record->token_lo = (uint32_t)nonce;
    record->token_hi = (uint32_t)(nonce >> 32);
    record->active = 0;
    record->reserved = 0;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    __atomic_store_n(&record->seq, 2, __ATOMIC_RELEASE);
    munmap(record, 32);
    close(fd);

    if (access(publisher, X_OK)) return 0;
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions)) return 0;
    posix_spawn_file_actions_addclose(&actions, game_lock);
    char *args[] = { publisher, mailbox, pid_text, token, NULL };
    pid_t child = 0;
    int rc = posix_spawn(&child, publisher, &actions, NULL, args, environ);
    posix_spawn_file_actions_destroy(&actions);
    if (rc) {
        fprintf(stderr, "Native mouse focus publisher could not start: %s\n", strerror(rc));
        return 0;
    }
    fprintf(stderr, "Native mouse focus publisher started (pid=%d, target=%d).\n", child, getpid());
    return 1;
}

int main(int argc, char **argv) {
    char app[PATH_MAX], support[PATH_MAX], contents[PATH_MAX];
    char state[PATH_MAX], prefix[PATH_MAX], game[PATH_MAX], wine[PATH_MAX];
    char logs[PATH_MAX], lockpath[PATH_MAX], path[PATH_MAX];
    const char *home = getenv("HOME");
    if (!home || home[0] != '/') {
        fprintf(stderr, "Не удалось определить домашнюю папку пользователя.\n");
        return 2;
    }
    if (!realpath(HOGS_ONLINE_APP_PATH, app)) {
        fprintf(stderr, "Не найдено приложение /Applications/Hogs of War.app.\n");
        return 2;
    }
    snprintf(contents, sizeof(contents), "%s/Contents", app);
    snprintf(support, sizeof(support), "%s/SharedSupport", contents);
    snprintf(state, sizeof(state), "%s/Library/Application Support/Hogs of War", home);
    snprintf(prefix, sizeof(prefix), "%s/prefix", state);
    snprintf(game, sizeof(game), "%s/drive_c/HogsofWar", prefix);
    snprintf(wine, sizeof(wine), "%s/wine/bin/wine", support);
    snprintf(logs, sizeof(logs), "%s/Logs", state);
    snprintf(lockpath, sizeof(lockpath), "%s/game.pid", state);

    if (argc == 2 && strcmp(argv[1], "--check") == 0) {
        if (!files_ok(support, game, wine)) {
            fprintf(stderr, "Не найдены файлы сетевой версии или компоненты Wine.\n");
            return 3;
        }
        if (!native_directplay_ready(prefix)) {
            fprintf(stderr, "Сетевая игра пока не подготовлена. Одиночный режим запускается прежним ярлыком.\n");
            return 7;
        }
        puts("Проверка файлов завершена. Игра не запускалась.");
        return 0;
    }
    if (!(argc == 2 && strcmp(argv[1], "--online") == 0)) {
        fprintf(stderr, "Запускай этот файл двойным щелчком или передай --check для проверки файлов.\n");
        return 2;
    }
    if (!files_ok(support, game, wine)) {
        fprintf(stderr, "Не найдены файлы сетевой версии. Проверь комплект приложения, затем повтори запуск.\n");
        return 3;
    }
    if (!native_directplay_ready(prefix)) {
        fprintf(stderr, "Сетевая игра пока не подготовлена. Одиночный режим запускается прежним ярлыком.\n");
        return 7;
    }
    if (hogs_is_running()) {
        fprintf(stderr, "Hogs of War уже запущена. Сначала выйди из игры обычным способом, затем запусти сетевой ярлык.\n");
        return 4;
    }

    mkdir(state, 0700);
    mkdir(logs, 0700);
    int lock = open(lockpath, O_RDWR | O_CREAT, 0600);
    if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB) != 0) {
        if (lock >= 0) close(lock);
        fprintf(stderr, "Игра уже запущена или запускается. Сетевой ярлык не стал открывать вторую копию.\n");
        return 4;
    }

    snprintf(path, sizeof(path), "%s/online.log", logs);
    int log = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (log >= 0) { dup2(log, STDOUT_FILENO); dup2(log, STDERR_FILENO); close(log); }

    snprintf(path, sizeof(path), "%s/Frameworks:/usr/lib", contents);
    setenv("WINEPREFIX", prefix, 1);
    setenv("DYLD_FALLBACK_LIBRARY_PATH", path, 1);
    setenv("WINEDLLOVERRIDES", "ddraw,d3dimm=n;d3d11,dxgi,d3d10core=b;dinput=n,b;dplayx,dpwsockx=n;wsock32,mswsock=b;mscoree,mshtml=", 1);
    setenv("WINEDEBUG", "-all,+loaddll", 1);
    setenv("MVK_CONFIG_LOG_LEVEL", "0", 1);
    setenv("DXVK_HUD", "fps", 1);
    setenv("DXVK_LOG_PATH", logs, 1);
    snprintf(path, sizeof(path), "%s/dxvk.conf", game);
    setenv("DXVK_CONFIG_FILE", path, 1);
    setenv("__COMPAT_LAYER", "DWM8And16BitMitigation HIGHDPIAWARE", 1);

    if (chdir(game) != 0) { perror("Не удалось открыть папку игры"); return 5; }
    if (!start_focus_publisher(contents, game, lock))
        fprintf(stderr, "Native mouse focus is unavailable; mouse input remains disabled safely.\n");
    char *args[] = { wine, "C:\\HogsofWar\\warhogs_.exe", NULL };
    execv(wine, args);
    perror("Не удалось запустить игру");
    return 6;
}
