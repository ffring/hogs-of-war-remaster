#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "gpu_profile_win.h"

#define APP_ID "389900"
#define ID_PATH 101
#define ID_BROWSE 102
#define ID_INSTALL 103
#define ID_RESTORE 104
#define ID_TAILSCALE 105
#define ID_STEAM 106
#define ID_HD_INSTALL 107
#define ID_HD_RESTORE 108
#define PATHBUF (MAX_PATH * 4)

extern const unsigned char hogsfix_dinput_start[], hogsfix_dinput_end[];
extern const unsigned char hogsfix_asi_start[], hogsfix_asi_end[];
extern const unsigned char hogsfix_ini_start[], hogsfix_ini_end[];
extern const unsigned char hogsfix_license_start[], hogsfix_license_end[];
extern const unsigned char hogsfix_readme_start[], hogsfix_readme_end[];
extern const unsigned char hogs_original_hashes_start[], hogs_original_hashes_end[];

static HINSTANCE g_instance;
static HWND g_path, g_status;
static const char *names[] = {"dinput.dll", "HogsFix.asi", "HogsFix.ini", "LICENSE-UltimateASILoader.txt", "HogsFix-README.txt"};
static const unsigned char *blobs[] = {hogsfix_dinput_start, hogsfix_asi_start, hogsfix_ini_start, hogsfix_license_start, hogsfix_readme_start};
static const unsigned char *blob_ends[] = {hogsfix_dinput_end, hogsfix_asi_end, hogsfix_ini_end, hogsfix_license_end, hogsfix_readme_end};
extern const unsigned char gpu_page_0_start[], gpu_page_0_end[];
extern const unsigned char gpu_page_1_start[], gpu_page_1_end[];
extern const unsigned char gpu_page_2_start[], gpu_page_2_end[];
extern const unsigned char gpu_page_3_start[], gpu_page_3_end[];
extern const unsigned char gpu_page_4_start[], gpu_page_4_end[];
extern const unsigned char gpu_page_5_start[], gpu_page_5_end[];
extern const unsigned char gpu_page_6_start[], gpu_page_6_end[];
extern const unsigned char gpu_page_7_start[], gpu_page_7_end[];
extern const unsigned char gpu_page_8_start[], gpu_page_8_end[];
extern const unsigned char gpu_page_9_start[], gpu_page_9_end[];
extern const unsigned char gpu_page_10_start[], gpu_page_10_end[];
extern const unsigned char gpu_page_11_start[], gpu_page_11_end[];
extern const unsigned char gpu_page_12_start[], gpu_page_12_end[];
extern const unsigned char gpu_page_13_start[], gpu_page_13_end[];
extern const unsigned char gpu_page_14_start[], gpu_page_14_end[];
extern const unsigned char gpu_page_15_start[], gpu_page_15_end[];
extern const unsigned char gpu_page_16_start[], gpu_page_16_end[];
extern const unsigned char gpu_page_17_start[], gpu_page_17_end[];
extern const unsigned char gpu_page_18_start[], gpu_page_18_end[];
extern const unsigned char gpu_page_19_start[], gpu_page_19_end[];
extern const unsigned char gpu_page_20_start[], gpu_page_20_end[];
extern const unsigned char gpu_page_21_start[], gpu_page_21_end[];
extern const unsigned char gpu_page_22_start[], gpu_page_22_end[];
extern const unsigned char gpu_page_23_start[], gpu_page_23_end[];
static const char *gpu_page_names[24] = {
    "29EF8CF01DE8C0C1.dds", "5553EA60D63CB1A2.dds", "5EA8B46DBBC2F6F1.dds", "5F3EF3188DD4F93C.dds", "6683762FEBEE7CD9.dds", "76FF785961C4EF2E.dds", "1D690CF10B6DA019.dds", "415B0F1CE2A20345.dds", "4513383B3674B42A.dds", "683EB3B52B0BCDBF.dds", "6C91225C31DCB094.dds", "6D16EB74289BFDEB.dds", "7BEB2430EEDA6DB3.dds", "82E6908652A274A5.dds", "85EC7F221BAA9B80.dds", "8D56A2A1130FBF7F.dds", "93BBA90B91481EF3.dds", "BAAE7913CA40F4BB.dds", "CA78ED331E52F69C.dds", "EA2776265AF6E3B7.dds", "F418E9A2947E3F22.dds", "F6E1C08D3C9C1491.dds", "F9BBB5040E79B576.dds", "FE083329CAD537B9.dds"
};
static const char *gpu_page_hashes[24] = {
    "c59fff53636e6dee82d91dc897589df9d9fd79c24da6504f60af9add33f2569e", "5e8cc8d59508bf16ee60e880b1f92d38a4b2d8d94619375208cfab834c6f2715", "e3475b275e4097541915eb161a5088acf68073b84f5d0e2074a57028807b1b9b", "631caf2d8b703005624cc2fb54032416eb3b2b292708ba110b480107fc8d8841", "9c49529f081645a8a90a8eb7f6147292217d07d99d33d49c2577f1653644c83b", "d19ee40eb461b39e9fd2c337f534a4a804fe4bd66498cbee3bd29fb22da653ce", "20f407b5bf9d52ac536a79914ba5858dc436a9dffa7d7bce717fa56906911cbc", "263d79711929e870ee44e633e62ecb12567983141e31e241d617a5b77828ca56", "7b2f0839232e943c851c986c4f76dc34de5172fba2abaa0749b66dd48400c635", "676e73a964f7c90c3a80d8958cc399842b1fc94a9340cfb696785bbf0d55d641", "3f7efb09746cc6fcdcc592472b1b9d084f172c7126d6769dfa4f20ec53df1855", "e5ea568621feb69501a578ac2f2cf16a57f3108be8ad3f4a0564bc9b2a34822d", "a41e6aa8ed1c3235704fcad11c8ebde84e1156765d4efacbc0328f80cf617730", "510c2a97a6eb61f90b9738273d2a9412eb7bfde3b9af7119c2ddd3766c7a2b41", "0aa7cb603ea9e45813b0e62142d4fbae9d4ba9540aebaae713d52068d40e7bca", "26d8f1595459b7ffc5bd83f3bd2a9db78dfa62a13bf16d76f7522ce61e777508", "486e599c79e781ccc1da3d3a6afbbe8add1eb96720981ea97daea3d7a06c8e9a", "09f55d46d2522149ea688d945112e67bfc668eb358fca668acbb22a441ef6987", "52d8817a5f1c1d73dd3c8c099776a64f17e5f2a9c09522a07148005cf05528f0", "59b5ed3d002890fe35330132236b974906f9aa97d3a31031c57280f22bf4f66d", "d211b7bdca41d4a4a733b1a7c63bbf78ef28b32559fc02c33c2bba5fb88839a8", "b42463a02c91f50e5fe591be86bb6de5f7637a4f4f8a1b01741ee58507e7a4c0", "a184a970fe3eec9762028fac5161f3be30ee8f1ab8d15b7c84722551d3be1075", "c394c34ac8cec47744d85a608e73477cc146b9f1fb745eb93bae15cdead14d00"
};
static const unsigned char *gpu_page_starts[24] = {
    gpu_page_0_start, gpu_page_1_start, gpu_page_2_start, gpu_page_3_start, gpu_page_4_start, gpu_page_5_start, gpu_page_6_start, gpu_page_7_start, gpu_page_8_start, gpu_page_9_start, gpu_page_10_start, gpu_page_11_start, gpu_page_12_start, gpu_page_13_start, gpu_page_14_start, gpu_page_15_start, gpu_page_16_start, gpu_page_17_start, gpu_page_18_start, gpu_page_19_start, gpu_page_20_start, gpu_page_21_start, gpu_page_22_start, gpu_page_23_start
};
static const unsigned char *gpu_page_ends[24] = {
    gpu_page_0_end, gpu_page_1_end, gpu_page_2_end, gpu_page_3_end, gpu_page_4_end, gpu_page_5_end, gpu_page_6_end, gpu_page_7_end, gpu_page_8_end, gpu_page_9_end, gpu_page_10_end, gpu_page_11_end, gpu_page_12_end, gpu_page_13_end, gpu_page_14_end, gpu_page_15_end, gpu_page_16_end, gpu_page_17_end, gpu_page_18_end, gpu_page_19_end, gpu_page_20_end, gpu_page_21_end, gpu_page_22_end, gpu_page_23_end
};
extern const unsigned char gpu_ddraw_start[], gpu_ddraw_end[];
extern const unsigned char gpu_d3dimm_start[], gpu_d3dimm_end[];
extern const unsigned char gpu_dgvoodoo_conf_start[], gpu_dgvoodoo_conf_end[];
extern const unsigned char gpu_toolkit_asi_start[], gpu_toolkit_asi_end[];
extern const unsigned char gpu_toolkit_license_start[], gpu_toolkit_license_end[];
extern const unsigned char gpu_dgvoodoo_license_start[], gpu_dgvoodoo_license_end[];


static wchar_t *wide(const char *s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (!n) return NULL;
    wchar_t *w = HeapAlloc(GetProcessHeap(), 0, (SIZE_T)n * sizeof(wchar_t));
    if (w) MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}
static void status(const char *s) { wchar_t *w=wide(s); if(w){SetWindowTextW(g_status,w);HeapFree(GetProcessHeap(),0,w);} }
static int exists(const char *p) { wchar_t *w=wide(p); if(!w)return 0; DWORD a=GetFileAttributesW(w); HeapFree(GetProcessHeap(),0,w); return a != INVALID_FILE_ATTRIBUTES; }
static int mkdir_new(const char *p) { wchar_t *w=wide(p); if(!w)return 0; BOOL ok=CreateDirectoryW(w,NULL); HeapFree(GetProcessHeap(),0,w); return ok; }
static FILE *open_utf8(const char *p, const wchar_t *mode) { wchar_t *w=wide(p); if(!w)return NULL; FILE *f=_wfopen(w,mode); HeapFree(GetProcessHeap(),0,w); return f; }
static int copy_file(const char *from,const char *to,BOOL no_overwrite){wchar_t*a=wide(from),*b=wide(to);if(!a||!b){if(a)HeapFree(GetProcessHeap(),0,a);if(b)HeapFree(GetProcessHeap(),0,b);return 0;}BOOL ok=CopyFileW(a,b,no_overwrite);HeapFree(GetProcessHeap(),0,a);HeapFree(GetProcessHeap(),0,b);return ok;}
static int delete_file(const char*p){wchar_t*w=wide(p);if(!w)return 0;BOOL ok=DeleteFileW(w);HeapFree(GetProcessHeap(),0,w);return ok;}

static int supported_steam_exe(const char *path) {
    wchar_t *wpath=wide(path); if(!wpath)return 0;
    HANDLE f = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HeapFree(GetProcessHeap(),0,wpath);
    if (f == INVALID_HANDLE_VALUE) return 0;
    DWORD got = 0, peoff = 0, sig = 0, stamp = 0, image_size = 0;
    LARGE_INTEGER pos; int ok = 0;
    if (ReadFile(f, &sig, 2, &got, NULL) && got == 2 && sig == 0x5a4d &&
        (pos.QuadPart = 0x3c, SetFilePointerEx(f, pos, NULL, FILE_BEGIN)) &&
        ReadFile(f, &peoff, 4, &got, NULL) && got == 4 && peoff < 0x100000 &&
        (pos.QuadPart = peoff, SetFilePointerEx(f, pos, NULL, FILE_BEGIN)) &&
        ReadFile(f, &sig, 4, &got, NULL) && got == 4 && sig == 0x00004550 &&
        (pos.QuadPart = peoff + 8, SetFilePointerEx(f, pos, NULL, FILE_BEGIN)) &&
        ReadFile(f, &stamp, 4, &got, NULL) && got == 4 &&
        (pos.QuadPart = peoff + 80, SetFilePointerEx(f, pos, NULL, FILE_BEGIN)) &&
        ReadFile(f, &image_size, 4, &got, NULL) && got == 4)
        ok = stamp == 0x3a069f75 && image_size == 0x0015e000;
    CloseHandle(f);
    return ok;
}

static int write_blob(const char *path, const unsigned char *data, size_t len) {
    wchar_t *wpath=wide(path); if(!wpath)return 0;
    HANDLE f = CreateFileW(wpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    HeapFree(GetProcessHeap(),0,wpath);
    if (f == INVALID_HANDLE_VALUE) return 0;
    DWORD wrote = 0;
    int ok = WriteFile(f, data, (DWORD)len, &wrote, NULL) && wrote == len;
    CloseHandle(f);
    return ok;
}

static int copy_blob_to_game(const char *dir, int i) {
    char dst[MAX_PATH];
    size_t len = (size_t)(blob_ends[i] - blobs[i]);
    if (snprintf(dst, sizeof(dst), "%s\\%s", dir, names[i]) >= (int)sizeof(dst)) return 0;
    return write_blob(dst, blobs[i], len);
}

static int parse_install_record(const char *dir, char *backup, size_t cap, int flags[5]) {
    char marker[PATHBUF], line[256];
    snprintf(marker, sizeof(marker), "%s\\.HogsFix-Multiplayer-Install.txt", dir);
    FILE *f = open_utf8(marker, L"rb");
    if (!f) return 0;
    int ok = fgets(line, sizeof(line), f) && !strcmp(line, "HOGSINSTALL2\n") &&
             fgets(line, sizeof(line), f);
    if (ok) {
        line[strcspn(line, "\r\n")] = 0;
        if (strncmp(line, ".HogsFix-backup-", 16) != 0 || strstr(line, "..") || strlen(line) + 1 > cap) ok = 0;
        else strcpy(backup, line);
    }
    if (ok && fgets(line, sizeof(line), f)) {
        ok = strlen(line) >= 5;
        for (int i=0; ok && i<5; i++) { ok = line[i] >= '0' && line[i] <= '1'; if(ok) flags[i] = line[i] - '0'; }
    } else ok = 0;
    fclose(f);
    return ok;
}

static int write_install_record(const char *dir, const char *backup, const int flags[5]) {
    char marker[PATHBUF], text[256];
    snprintf(marker, sizeof(marker), "%s\\.HogsFix-Multiplayer-Install.txt", dir);
    snprintf(text, sizeof(text), "HOGSINSTALL2\n%s\n%d%d%d%d%d\n", backup, flags[0], flags[1], flags[2], flags[3], flags[4]);
    FILE *f = open_utf8(marker, L"wb");
    if (!f) return 0;
    int ok = fwrite(text, 1, strlen(text), f) == strlen(text);
    fclose(f);
    return ok;
}

static int hogs_game_running(void) {
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snap==INVALID_HANDLE_VALUE)return -1;
    PROCESSENTRY32W e;memset(&e,0,sizeof(e));e.dwSize=sizeof(e);int found=0;
    if(Process32FirstW(snap,&e)){do{if(!_wcsicmp(e.szExeFile,L"warhogs_.exe")){found=1;break;}}while(Process32NextW(snap,&e));}
    else if(GetLastError()!=ERROR_NO_MORE_FILES){CloseHandle(snap);return -1;}
    CloseHandle(snap);return found;
}
static int profile_marker_exists(const char *dir) { char p[PATHBUF]; return snprintf(p,sizeof(p),"%s\\.Hogs-GPU-HD-Profile.txt",dir)<(int)sizeof(p) && exists(p); }
static int fix_marker_exists(const char *dir) { char p[PATHBUF]; return snprintf(p,sizeof(p),"%s\\.HogsFix-Multiplayer-Install.txt",dir)<(int)sizeof(p) && exists(p); }
static void gpu_install(void) {
    int running=hogs_game_running();if(running!=0){status(running>0?"Сначала закрой Hogs of War, затем установи HD-профиль.":"Не удалось проверить, запущена ли игра. Повтори позже.");return;}
    char dir[PATHBUF],msg[512],exe[PATHBUF];wchar_t wdir[MAX_PATH];GetWindowTextW(g_path,wdir,MAX_PATH);
    if(!WideCharToMultiByte(CP_UTF8,0,wdir,-1,dir,sizeof(dir),NULL,NULL)){status("Слишком длинный путь к папке игры.");return;}
    if(snprintf(exe,sizeof(exe),"%s\\warhogs_.exe",dir)>=(int)sizeof(exe)||!supported_steam_exe(exe)){status("Выбери папку поддерживаемой Steam-версии игры.");return;}
    if(!fix_marker_exists(dir)){status("Сначала нажми «Установить исправления», затем установи HD-профиль.");return;}
    HogsProfileFile files[30];static char rel[30][96];
    for(int i=0;i<24;i++){snprintf(rel[i],sizeof(rel[i]),"TT\\inject\\%s",gpu_page_names[i]);files[i]=(HogsProfileFile){rel[i],gpu_page_starts[i],(size_t)(gpu_page_ends[i]-gpu_page_starts[i]),gpu_page_hashes[i]};}
    const unsigned char *starts[6]={gpu_ddraw_start,gpu_d3dimm_start,gpu_dgvoodoo_conf_start,gpu_toolkit_asi_start,gpu_toolkit_license_start,gpu_dgvoodoo_license_start};
    const unsigned char *ends[6]={gpu_ddraw_end,gpu_d3dimm_end,gpu_dgvoodoo_conf_end,gpu_toolkit_asi_end,gpu_toolkit_license_end,gpu_dgvoodoo_license_end};
    const char *names[6]={"DDraw.dll","D3DImm.dll","dgVoodoo.conf","TextureToolkit-x86.asi","LICENSE-TextureToolkit.txt","DgVoodoo-Redistribution.txt"};
    for(int i=0;i<6;i++){snprintf(rel[24+i],sizeof(rel[24+i]),"%s",names[i]);files[24+i]=(HogsProfileFile){rel[24+i],starts[i],(size_t)(ends[i]-starts[i]),NULL};}
    hogs_gpu_profile_install(dir,hogs_original_hashes_start,(size_t)(hogs_original_hashes_end-hogs_original_hashes_start),files,30,msg,sizeof(msg));status(msg);
}
static void gpu_restore(void) {
    int running=hogs_game_running();if(running!=0){status(running>0?"Сначала закрой Hogs of War, затем восстанови HD-профиль.":"Не удалось проверить, запущена ли игра. Повтори позже.");return;}
    char dir[PATHBUF],msg[512];wchar_t wdir[MAX_PATH];GetWindowTextW(g_path,wdir,MAX_PATH);
    if(!WideCharToMultiByte(CP_UTF8,0,wdir,-1,dir,sizeof(dir),NULL,NULL)){status("Слишком длинный путь к папке игры.");return;}
    hogs_gpu_profile_restore(dir,NULL,0,msg,sizeof(msg));status(msg);
}

static int install_fix(void) {
    int running=hogs_game_running();if(running!=0){status(running>0?"Сначала закрой Hogs of War, затем установи исправления.":"Не удалось проверить, запущена ли игра. Повтори позже.");return 0;}
    char dir[PATHBUF], backup[PATHBUF], backup_name[64], dst[PATHBUF], src[PATHBUF];
    int flags[5] = {0,0,0,0,0};
    wchar_t wdir[MAX_PATH]; GetWindowTextW(g_path,wdir,MAX_PATH);
    if (!WideCharToMultiByte(CP_UTF8,0,wdir,-1,dir,sizeof(dir),NULL,NULL)) { status("Слишком длинный путь к папке игры."); return 0; }
    if (!exists(dir) || snprintf(dst, sizeof(dst), "%s\\warhogs_.exe", dir) >= (int)sizeof(dst) || !supported_steam_exe(dst)) {
        status("Не нашёл поддерживаемую версию игры. Выбери папку Hogs of War из Steam."); return 0;
    }
    if (!hogs_archive_guard_check(dir,hogs_original_hashes_start,(size_t)(hogs_original_hashes_end-hogs_original_hashes_start),dst,sizeof(dst))) { status(dst); return 0; }
    snprintf(dst, sizeof(dst), "%s\\.HogsFix-Multiplayer-Install.txt", dir);
    int has_record=parse_install_record(dir, backup_name, sizeof(backup_name), flags);
    if (exists(dst) && !has_record) {
        status("Данные установки повреждены. Ничего не менял."); return 0;
    }
    if (has_record) {
        snprintf(backup, sizeof(backup), "%s\\%s", dir, backup_name);
        for (int i=0;i<5;i++) if (flags[i]) {
            snprintf(src, sizeof(src), "%s\\%s", backup, names[i]);
            if (!exists(src)) { status("Не найдена резервная копия. Файлы игры не менял."); return 0; }
        }
    } else {
        SYSTEMTIME t; GetLocalTime(&t); int made=0;
        for (unsigned attempt=0;attempt<100;attempt++) {
            snprintf(backup_name, sizeof(backup_name), ".HogsFix-backup-%04u%02u%02u-%02u%02u%02u-%02u", t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,attempt);
            snprintf(backup, sizeof(backup), "%s\\%s", dir, backup_name);
            if (mkdir_new(backup)) { made=1; break; }
            if (GetLastError()!=ERROR_ALREADY_EXISTS) break;
        }
        if (!made) { status("Не удалось создать папку для резервной копии."); return 0; }
        for (int i=0;i<5;i++) {
            snprintf(dst, sizeof(dst), "%s\\%s", dir, names[i]);
            if (exists(dst)) {
                snprintf(src, sizeof(src), "%s\\%s", backup, names[i]);
                if (!copy_file(dst, src, TRUE)) { status("Не удалось сохранить исходный файл. Игра не изменена."); return 0; }
                flags[i] = 1;
            }
        }
        if (!write_install_record(dir, backup_name, flags)) { status("Не удалось записать данные резервной копии. Игра не изменена."); return 0; }
    }
    for (int i=0;i<5;i++) if (!copy_blob_to_game(dir, i)) {
        status("Копирование не завершено. Возможно, нужны права на запись: запусти установщик от имени администратора."); return 0;
    }
    status("Готово. Запусти Hogs of War из Steam. Исходные файлы сохранены рядом, если они были.");
    return 1;
}

static int restore_fix(void) {
    int running=hogs_game_running();if(running!=0){status(running>0?"Сначала закрой Hogs of War, затем восстанови файлы.":"Не удалось проверить, запущена ли игра. Повтори позже.");return 0;}
    char dir[PATHBUF], backup_name[128], backup[PATHBUF], marker[PATHBUF], path[PATHBUF], from[PATHBUF];
    int flags[5] = {0,0,0,0,0};
    wchar_t wdir[MAX_PATH]; GetWindowTextW(g_path,wdir,MAX_PATH);
    if (!WideCharToMultiByte(CP_UTF8,0,wdir,-1,dir,sizeof(dir),NULL,NULL)) { status("Слишком длинный путь к папке игры."); return 0; }
    if (profile_marker_exists(dir)) { status("Сначала нажми «Восстановить графику», затем восстанови исходные файлы."); return 0; }
    if (!parse_install_record(dir, backup_name, sizeof(backup_name), flags)) { status("Для этой папки не найдено резервной копии установщика."); return 0; }
    snprintf(backup, sizeof(backup), "%s\\%s", dir, backup_name);
    for (int i=0;i<5;i++) {
        snprintf(path, sizeof(path), "%s\\%s", dir, names[i]);
        if (flags[i]) {
            snprintf(from, sizeof(from), "%s\\%s", backup, names[i]);
            if (!exists(from) || !copy_file(from, path, FALSE)) { status("Не удалось восстановить исходный файл. Резервная копия сохранена."); return 0; }
        } else if (exists(path) && !delete_file(path)) { status("Не удалось удалить файл HogsFix. Запусти установщик от имени администратора."); return 0; }
    }
    snprintf(marker, sizeof(marker), "%s\\.HogsFix-Multiplayer-Install.txt", dir);
    delete_file(marker);
    for (int i=0;i<5;i++) { snprintf(path, sizeof(path), "%s\\%s", backup, names[i]); delete_file(path); }
    wchar_t *wb=wide(backup); if(wb){RemoveDirectoryW(wb);HeapFree(GetProcessHeap(),0,wb);}
    status("Исходные файлы восстановлены.");
    return 1;
}

static int path_from_manifest(const char *steamapps, char *game, size_t cap) {
    char manifest[PATHBUF], line[1024], install_dir[256] = "";
    snprintf(manifest, sizeof(manifest), "%s\\appmanifest_" APP_ID ".acf", steamapps);
    FILE *f = open_utf8(manifest, L"rb");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        char *k = strstr(line, "\"installdir\"");
        if (k) { char *a = strchr(k + 12, '"'); if (a) { char *b = strchr(a + 1, '"'); if (b && (size_t)(b-a-1) < sizeof(install_dir)) { memcpy(install_dir, a+1, b-a-1); install_dir[b-a-1]=0; break; } } }
    }
    fclose(f);
    if (!install_dir[0]) return 0;
    char common[PATHBUF]; snprintf(common, sizeof(common), "%s\\common\\%s", steamapps, install_dir);
    snprintf(manifest, sizeof(manifest), "%s\\warhogs_.exe", common);
    if (!exists(manifest) || strlen(common)+1 > cap) return 0;
    strcpy(game, common); return 1;
}

static int try_steam(const char *root, char *game, size_t cap) {
    char apps[PATHBUF], libs[PATHBUF], line[1024], candidate[PATHBUF];
    snprintf(apps, sizeof(apps), "%s\\steamapps", root);
    if (path_from_manifest(apps, game, cap)) return 1;
    snprintf(libs, sizeof(libs), "%s\\libraryfolders.vdf", apps);
    FILE *f = open_utf8(libs, L"rb");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            char *p = strstr(line, "\"path\"");
            if (!p) continue;
            char *a = strchr(p + 6, '"'); if (!a) continue;
            char *b = strchr(a+1, '"'); if (!b || (size_t)(b-a-1) >= sizeof(candidate)) continue;
            memcpy(candidate, a+1, b-a-1); candidate[b-a-1]=0;
            for (char *q=candidate; *q; q++) if (*q=='\\' && q[1]=='\\') memmove(q+1,q+2,strlen(q+2)+1);
            snprintf(apps, sizeof(apps), "%s\\steamapps", candidate);
            if (path_from_manifest(apps, game, cap)) { fclose(f); return 1; }
        }
        fclose(f);
    }
    snprintf(candidate, sizeof(candidate), "%s\\common\\Hogs of War", apps);
    snprintf(line, sizeof(line), "%s\\warhogs_.exe", candidate);
    if (exists(line) && strlen(candidate)+1 < cap) { strcpy(game,candidate); return 1; }
    return 0;
}

static void find_steam_game(char *game, size_t cap) {
    char root[PATHBUF]; DWORD n;
    const char *envs[] = {"ProgramFiles(x86)", "ProgramFiles", "LOCALAPPDATA"};
    for (int i=0;i<3;i++) {
        wchar_t key[64], wroot[MAX_PATH]; MultiByteToWideChar(CP_UTF8,0,envs[i],-1,key,64);
        n = GetEnvironmentVariableW(key, wroot, MAX_PATH); if (!n || n >= MAX_PATH) continue;
        if (!WideCharToMultiByte(CP_UTF8,0,wroot,-1,root,sizeof(root),NULL,NULL)) continue;
        if (i == 2) strncat(root, "\\Programs\\Steam", sizeof(root)-strlen(root)-1);
        else strncat(root, "\\Steam", sizeof(root)-strlen(root)-1);
        if (try_steam(root, game, cap)) return;
    }
    game[0] = 0;
}

static void browse_game(void) {
    BROWSEINFOW bi = {0}; bi.hwndOwner = GetActiveWindow(); bi.lpszTitle = L"Выбери папку Hogs of War из Steam"; bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST id = SHBrowseForFolderW(&bi);
    if (id) { wchar_t path[MAX_PATH]; if (SHGetPathFromIDListW(id, path)) SetWindowTextW(g_path, path); CoTaskMemFree(id); }
}

static LRESULT CALLBACK wndproc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_CREATE: {
        CreateWindowW(L"STATIC", L"Папка игры Steam", WS_CHILD|WS_VISIBLE, 18,18,200,22,w,NULL,g_instance,NULL);
        g_path = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,18,43,435,28,w,(HMENU)ID_PATH,g_instance,NULL);
        CreateWindowW(L"BUTTON",L"Обзор…",WS_CHILD|WS_VISIBLE,464,42,94,30,w,(HMENU)ID_BROWSE,g_instance,NULL);
        CreateWindowW(L"BUTTON",L"Установить исправления",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,18,88,260,34,w,(HMENU)ID_INSTALL,g_instance,NULL);
        CreateWindowW(L"BUTTON",L"Восстановить исходные файлы",WS_CHILD|WS_VISIBLE,290,88,268,34,w,(HMENU)ID_RESTORE,g_instance,NULL);
        CreateWindowW(L"BUTTON",L"Установить HD-профиль",WS_CHILD|WS_VISIBLE,18,132,260,34,w,(HMENU)ID_HD_INSTALL,g_instance,NULL);
        CreateWindowW(L"BUTTON",L"Восстановить графику",WS_CHILD|WS_VISIBLE,290,132,268,34,w,(HMENU)ID_HD_RESTORE,g_instance,NULL);
        CreateWindowW(L"STATIC",L"HD-профиль заменяет 24 страницы через dgVoodoo и Direct3D 11.",WS_CHILD|WS_VISIBLE,18,176,540,24,w,NULL,g_instance,NULL);
        CreateWindowW(L"STATIC",L"Для игры с другом настройте Tailscale на обоих компьютерах.",WS_CHILD|WS_VISIBLE,18,208,540,24,w,NULL,g_instance,NULL);
        CreateWindowW(L"BUTTON",L"Скачать Tailscale…",WS_CHILD|WS_VISIBLE,18,240,200,32,w,(HMENU)ID_TAILSCALE,g_instance,NULL);
        CreateWindowW(L"BUTTON",L"Открыть Hogs of War в Steam",WS_CHILD|WS_VISIBLE,228,240,230,32,w,(HMENU)ID_STEAM,g_instance,NULL);
        g_status = CreateWindowW(L"STATIC",L"Перед установкой закрой игру. Сетевая игра между двумя компьютерами ещё не проверена.",WS_CHILD|WS_VISIBLE,18,284,540,48,w,NULL,g_instance,NULL);
        char path[PATHBUF]; find_steam_game(path,sizeof(path)); if (path[0]) { wchar_t *wp=wide(path); if(wp){SetWindowTextW(g_path,wp);HeapFree(GetProcessHeap(),0,wp);} }
        return 0; }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_BROWSE: browse_game(); return 0;
        case ID_INSTALL: install_fix(); return 0;
        case ID_RESTORE: restore_fix(); return 0;
        case ID_HD_INSTALL: gpu_install(); return 0;
        case ID_HD_RESTORE: gpu_restore(); return 0;
        case ID_TAILSCALE:
            if ((INT_PTR)ShellExecuteW(w,L"open",L"https://tailscale.com/download/windows",NULL,NULL,SW_SHOWNORMAL)>32)
                status("Открыл официальную страницу Tailscale. Скачай программу и установи её сам.");
            else status("Не удалось открыть страницу Tailscale. Проверь подключение к интернету.");
            return 0;
        case ID_STEAM: ShellExecuteW(w,L"open",L"https://store.steampowered.com/app/389900/",NULL,NULL,SW_SHOWNORMAL); return 0;
        }
        break;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(w,m,wp,lp);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmd, int show) {
    (void)prev; (void)cmd; g_instance = instance;
    INITCOMMONCONTROLSEX ic = {sizeof(ic), ICC_STANDARD_CLASSES}; InitCommonControlsEx(&ic); CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    WNDCLASSW wc = {0}; wc.lpfnWndProc=wndproc; wc.hInstance=instance; wc.lpszClassName=L"HogsWarSetupWindow"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); RegisterClassW(&wc);
    HWND w=CreateWindowW(wc.lpszClassName,L"Hogs of War — настройка игры",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,584,372,NULL,NULL,instance,NULL);
    if (!w) return 1; ShowWindow(w,show); UpdateWindow(w);
    MSG msg; while(GetMessageW(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);} CoUninitialize(); return (int)msg.wParam;
}
