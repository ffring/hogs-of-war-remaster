#define HOGSMOUSE_TEST 1
#include "../native_ini.h"

static int write_bytes(const char *p, const void *data, DWORD n) {
    HANDLE h=CreateFileA(p,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL); DWORD wr=0;
    if(h==INVALID_HANDLE_VALUE)return 0;
    int ok=WriteFile(h,data,n,&wr,NULL)&&wr==n; CloseHandle(h); return ok;
}
static int read_bytes(const char *p, BYTE *buf, DWORD cap, DWORD *got) {
    HANDLE h=CreateFileA(p,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL); DWORD n=0;
    if(h==INVALID_HANDLE_VALUE)return 0;
    int ok=ReadFile(h,buf,cap,&n,NULL); CloseHandle(h); *got=n; return ok;
}
static int same_file(const char *p,const BYTE *want,DWORD n) { BYTE b[65536]; DWORD got=0; return read_bytes(p,b,sizeof(b),&got)&&got==n&&!memcmp(b,want,n); }
static int visible_reparse(const char *p) {
    DWORD attrs=GetFileAttributesA(p);
    if(attrs!=INVALID_FILE_ATTRIBUTES && (attrs&FILE_ATTRIBUTE_REPARSE_POINT))return 1;
    HANDLE h=CreateFileA(p,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                         NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,NULL);
    if(h==INVALID_HANDLE_VALUE)return 0;
    FILE_ATTRIBUTE_TAG_INFO tag; int ok=GetFileInformationByHandleEx(h,FileAttributeTagInfo,&tag,sizeof(tag)) &&
        (tag.FileAttributes&FILE_ATTRIBUTE_REPARSE_POINT);
    CloseHandle(h); return ok;
}
static int fail(const char *s) { printf("FAIL %s\n",s); return 1; }
#define CHECK(x,msg) do { if(!(x)) return fail(msg); } while(0)

int main(void) {
    char root[MAX_PATH], dir[MAX_PATH], path[MAX_PATH], linkpath[MAX_PATH];
    DWORD n=GetTempPathA(sizeof(root),root); CHECK(n&&n<sizeof(root),"temp path");
    _snprintf(dir,sizeof(dir),"%shogs-native-ini-%08lx",root,(unsigned long)GetTickCount());
    CHECK(CreateDirectoryA(dir,NULL),"create fixture folder");
    _snprintf(path,sizeof(path),"%s\\HogsMouse.ini",dir);
    _snprintf(linkpath,sizeof(linkpath),"%s\\link.ini",dir);
    const char initial[]="; keep this comment\r\n[Other]\r\nKeep=hello\r\n[Mouse]\r\nEnable=1\r\nNativeUnitsPerPixel=4\r\nAimSensitivityPercent=100\r\nInvertX=0\r\nInvertY=0\r\n";
    CHECK(write_bytes(path,initial,sizeof(initial)-1),"write original fixture");
    HogsMouseSettings s;
    CHECK(hogs_ini_load(path,&s)==HOGS_INI_OK && s.native_units_per_pixel==4 && s.aim_sensitivity_percent==100 && !s.invert_x && !s.invert_y,"load existing values");
    s.native_units_per_pixel=12; s.aim_sensitivity_percent=300; s.invert_x=1; s.invert_y=1;
    CHECK(hogs_ini_save(path,&s)==HOGS_INI_OK,"save existing settings");
    CHECK(hogs_ini_load(path,&s)==HOGS_INI_OK && s.native_units_per_pixel==12 && s.aim_sensitivity_percent==300 && s.invert_x && s.invert_y,"read back four values");
    BYTE saved[65536]; DWORD saved_n=0; CHECK(read_bytes(path,saved,sizeof(saved)-1,&saved_n),"read committed file"); saved[saved_n]=0;
    CHECK(strstr((char*)saved,"; keep this comment") && strstr((char*)saved,"Keep=hello") && strstr((char*)saved,"Enable=1"),"preserve comments and unknown keys");
    HogsIniOps ops={HOGS_INI_TEST_FAIL_PRECOMMIT}; s.aim_sensitivity_percent=350;
    CHECK(hogs_ini_save_test(path,&s,&ops)==HOGS_INI_COMMIT_FAILED,"precommit injected failure reported");
    CHECK(same_file(path,saved,saved_n),"precommit failure preserves exact bytes");
    ops.test_failures=HOGS_INI_TEST_FAIL_POSTCOMMIT;
    CHECK(hogs_ini_save_test(path,&s,&ops)==HOGS_INI_READBACK_FAILED,"postcommit injected failure reported");
    CHECK(same_file(path,saved,saved_n),"postcommit rollback restores exact bytes");
    ops.test_failures=HOGS_INI_TEST_MUTATE_BEFORE_COMMIT;
    CHECK(hogs_ini_save_test(path,&s,&ops)==HOGS_INI_COMMIT_FAILED,"concurrent existing-target change refuses commit");
    static const char external[]="; external concurrent writer\r\n";
    CHECK(same_file(path,(const BYTE *)external,sizeof(external)-1),"existing concurrent writer bytes are preserved");
    CHECK(write_bytes(path,saved,saved_n),"restore fixture baseline after concurrency test");
    char racepath[MAX_PATH]; _snprintf(racepath,sizeof(racepath),"%s\\raced.ini",dir);
    CHECK(GetFileAttributesA(racepath)==INVALID_FILE_ATTRIBUTES,"missing race target starts absent");
    CHECK(hogs_ini_save_test(racepath,&s,&ops)==HOGS_INI_COMMIT_FAILED,"concurrent create of missing target refuses commit");
    CHECK(same_file(racepath,(const BYTE *)external,sizeof(external)-1),"concurrent create bytes are preserved");
    CHECK(DeleteFileA(racepath),"remove concurrent-create fixture");
    ops.test_failures=0;
    HogsMouseSettings invalid=s; invalid.native_units_per_pixel=25;
    CHECK(hogs_ini_save(path,&invalid)==HOGS_INI_BAD_ARGUMENT,"reject out-of-range save");
    CHECK(same_file(path,saved,saved_n),"invalid save preserves exact bytes");
    const char malformed[]="[Mouse]\r\nNativeUnitsPerPixel=99\r\n";
    CHECK(write_bytes(path,malformed,sizeof(malformed)-1),"write malformed fixture");
    CHECK(hogs_ini_load(path,&s)==HOGS_INI_MALFORMED,"malformed range rejected");
    HogsMouseSettings defaults=hogs_ini_defaults();
    CHECK(GetFileAttributesA(linkpath)==INVALID_FILE_ATTRIBUTES,"sanity nonexistent path");
    CHECK(hogs_ini_save(linkpath,&defaults)==HOGS_INI_OK,"create absent ini from defaults");
    CHECK(hogs_ini_load(linkpath,&s)==HOGS_INI_OK&&hogs_ini_same(&s,&defaults),"load created defaults");
    CHECK(DeleteFileA(linkpath),"remove absent-file fixture");
    if (CreateSymbolicLinkA(linkpath,path,SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) {
        if (visible_reparse(linkpath)) {
            CHECK(hogs_ini_load(linkpath,&s)==HOGS_INI_REPARSE_REFUSED,"refuse reparse-point ini");
            CHECK(hogs_ini_save(linkpath,&defaults)==HOGS_INI_REPARSE_REFUSED,"save refuses reparse-point ini");
            CHECK(same_file(path,(const BYTE *)malformed,sizeof(malformed)-1),"reparse refusal leaves target bytes unchanged");
        } else {
            puts("UNSUPPORTED symlink test: API reported creation but the reparse point was not observable");
        }
        DeleteFileA(linkpath);
    } else if (GetLastError()!=ERROR_PRIVILEGE_NOT_HELD && GetLastError()!=ERROR_INVALID_PARAMETER && GetLastError()!=ERROR_NOT_SUPPORTED) {
        return fail("create symbolic link fixture");
    }
    CHECK(DeleteFileA(path),"remove ini"); CHECK(RemoveDirectoryA(dir),"remove fixture folder");
    puts("PASS load/save, comments/unknown keys, malformed rejection, concurrent target-change refusal, pre/postcommit byte-exact rollback, create absent file");
    return 0;
}
