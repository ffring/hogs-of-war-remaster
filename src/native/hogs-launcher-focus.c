#include <mach-o/dyld.h>
#include <libproc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <spawn.h>
#include <sys/mman.h>
#include "focus_mailbox.h"
extern char **environ;



static int start_focus_publisher(const char *contents, const char *game, int game_lock) {
    char publisher[PATH_MAX], mailbox[PATH_MAX], token[17], pid_text[32];
    uint64_t nonce=0;
    snprintf(publisher,sizeof(publisher),"%s/MacOS/hogs-focus-publisher",contents);
    snprintf(mailbox,sizeof(mailbox),"%s/HogsMouse.focus",game);
    arc4random_buf(&nonce,sizeof(nonce)); if(!nonce)nonce=1;
    snprintf(token,sizeof(token),"%016llx",(unsigned long long)nonce);
    snprintf(pid_text,sizeof(pid_text),"%d",getpid());
    setenv("HOGS_MAC_FOCUS_REQUIRED","1",1);
    setenv("HOGS_MAC_FOCUS_FILE","C:\\HogsofWar\\HogsMouse.focus",1);
    setenv("HOGS_MAC_FOCUS_TOKEN",token,1);
    int fd=open(mailbox,O_RDWR|O_CREAT|O_NOFOLLOW|O_CLOEXEC,0600);
    if(fd<0)return 0;
    struct stat st;
    if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_uid!=getuid()||fchmod(fd,0600)||ftruncate(fd,32)){close(fd);return 0;}
    HMMacFocusRecord *p=mmap(NULL,32,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if(p==MAP_FAILED){close(fd);return 0;}
    p->seq=1; __atomic_thread_fence(__ATOMIC_SEQ_CST);
    p->magic=HM_MAC_FOCUS_MAGIC;p->version=HM_MAC_FOCUS_VERSION;p->native_game_pid=(uint32_t)getpid();
    p->token_lo=(uint32_t)nonce;p->token_hi=(uint32_t)(nonce>>32);p->active=0;p->reserved=0;
    __atomic_thread_fence(__ATOMIC_RELEASE); __atomic_store_n(&p->seq,2,__ATOMIC_RELEASE);
    munmap(p,32);close(fd);
    if(access(publisher,X_OK))return 0;
    posix_spawn_file_actions_t actions;
    if(posix_spawn_file_actions_init(&actions))return 0;
    posix_spawn_file_actions_addclose(&actions,game_lock);
    char *args[]={publisher,mailbox,pid_text,token,NULL};
    pid_t child=0;int rc=posix_spawn(&child,publisher,&actions,NULL,args,environ);
    posix_spawn_file_actions_destroy(&actions);
    if(rc){fprintf(stderr,"Native mouse focus publisher could not start: %s\n",strerror(rc));return 0;}
    fprintf(stderr,"Native mouse focus publisher started (pid=%d, target=%d).\n",child,getpid());
    return 1;
}

int main(int argc, char **argv) {
    char exe[PATH_MAX], resolved[PATH_MAX], contents[PATH_MAX];
    uint32_t n=sizeof(exe);
    if (_NSGetExecutablePath(exe,&n) || !realpath(exe,resolved)) return 1;
    char *slash=strrchr(resolved,'/'); if(!slash)return 1; *slash=0;
    slash=strrchr(resolved,'/'); if(!slash)return 1; *slash=0;
    snprintf(contents,sizeof(contents),"%s",resolved);
    char support[PATH_MAX],state[PATH_MAX],prefix[PATH_MAX],host[PATH_MAX],game[PATH_MAX],logs[PATH_MAX],libs[PATH_MAX],conf[PATH_MAX];
    snprintf(support,sizeof(support),"%s/SharedSupport",contents);
    const char *home=getenv("HOME");if(!home || *home!='/')return 1;
    snprintf(state,sizeof(state),"%s/Library/Application Support/Hogs of War",home);
    snprintf(prefix,sizeof(prefix),"%s/prefix",state);
    snprintf(host,sizeof(host),"%s/wine/bin/wine",support);
    snprintf(game,sizeof(game),"%s/drive_c/HogsofWar",prefix);
    snprintf(logs,sizeof(logs),"%s/Logs",state);
    snprintf(libs,sizeof(libs),"%s/Frameworks:/usr/lib",contents);
    snprintf(conf,sizeof(conf),"%s/dxvk.conf",game);
    if(argc>1 && !strcmp(argv[1],"--check")) {
        char p[PATH_MAX];
        const char *required[]={"warhogs_.exe","HogsFix.asi","HogsFix.ini","dinput.dll","ddraw.dll","d3dimm.dll","d3d11.dll","dxgi.dll","dgVoodoo.conf","dxvk.conf"};
        if(access(host,X_OK))return 2;
        snprintf(p,sizeof(p),"%s/MacOS/hogs-focus-publisher",contents);
        if(access(p,X_OK)){fprintf(stderr,"Missing native mouse focus publisher.\n");return 3;}
        for(unsigned i=0;i<sizeof(required)/sizeof(*required);i++){
            snprintf(p,sizeof(p),"%s/%s",game,required[i]);
            if(access(p,R_OK)){fprintf(stderr,"Missing: %s\n",required[i]);return 3;}
        }
        puts("Launcher and required files are present. No game was started.");return 0;
    }
    mkdir(state,0700);mkdir(logs,0700);
    char lockpath[PATH_MAX];snprintf(lockpath,sizeof(lockpath),"%s/game.pid",state);
    int lock=open(lockpath,O_RDWR|O_CREAT,0600);if(lock<0)return 4;
    if(flock(lock,LOCK_EX|LOCK_NB)){close(lock);return 0;}
    char b[64]={0};read(lock,b,sizeof(b)-1);pid_t old=(pid_t)strtol(b,NULL,10);
    char oldpath[PROC_PIDPATHINFO_MAXSIZE]={0};
    if(old>0 && old!=getpid() && !kill(old,0) && proc_pidpath(old,oldpath,sizeof(oldpath))>0 && !strcmp(oldpath,host)) {
        close(lock);return 0;
    }
    ftruncate(lock,0);lseek(lock,0,SEEK_SET);dprintf(lock,"%d\n",getpid());fsync(lock);
    /* Keep the lock across exec; the kernel releases it when this process exits. */
    int graphics_check=argc==2 && !strcmp(argv[1],"--graphics-check");
    if(argc>1 && !graphics_check)return 7;
    char logfile[PATH_MAX];snprintf(logfile,sizeof(logfile),"%s/%s",logs,graphics_check?"graphics-check.log":"game.log");
    int log=open(logfile,O_WRONLY|O_CREAT|O_TRUNC,0600);
    if(log>=0){dup2(log,STDOUT_FILENO);dup2(log,STDERR_FILENO);close(log);}
    setenv("WINEPREFIX",prefix,1);
    setenv("DYLD_FALLBACK_LIBRARY_PATH",libs,1);
    setenv("WINEDLLOVERRIDES","ddraw,d3dimm=n;d3d11,dxgi,d3d10core=b;dinput=n,b;mscoree,mshtml=",1);
    setenv("WINEDEBUG","-all,+loaddll",1);
    setenv("MVK_CONFIG_LOG_LEVEL","0",1);
    setenv("DXVK_HUD","fps",1);
    setenv("DXVK_LOG_PATH",logs,1);
    setenv("DXVK_CONFIG_FILE",conf,1);
    setenv("__COMPAT_LAYER","DWM8And16BitMitigation HIGHDPIAWARE",1);
    if(chdir(game))return 5;
    if(!graphics_check && !start_focus_publisher(contents,game,lock))
        fprintf(stderr,"Native mouse focus is unavailable; mouse input remains disabled safely.\n");
    char *args[]={host,graphics_check?"C:\\HogsofWar\\gpu-probe.exe":"C:\\HogsofWar\\warhogs_.exe",NULL};
    execv(host,args);
    perror("Launch failed");return 6;
}
