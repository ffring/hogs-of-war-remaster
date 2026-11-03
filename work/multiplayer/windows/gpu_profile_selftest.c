#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "gpu_profile_win.h"

extern const unsigned char gpu_ddraw_start[], gpu_ddraw_end[];
extern const unsigned char gpu_d3dimm_start[], gpu_d3dimm_end[];
extern const unsigned char gpu_dgvoodoo_conf_start[], gpu_dgvoodoo_conf_end[];
extern const unsigned char gpu_toolkit_asi_start[], gpu_toolkit_asi_end[];
extern const unsigned char gpu_toolkit_license_start[], gpu_toolkit_license_end[];
extern const unsigned char gpu_dgvoodoo_license_start[], gpu_dgvoodoo_license_end[];
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

static const char *page_names[24]={
    "29EF8CF01DE8C0C1.dds", "5553EA60D63CB1A2.dds", "5EA8B46DBBC2F6F1.dds", "5F3EF3188DD4F93C.dds", "6683762FEBEE7CD9.dds", "76FF785961C4EF2E.dds", "1D690CF10B6DA019.dds", "415B0F1CE2A20345.dds", "4513383B3674B42A.dds", "683EB3B52B0BCDBF.dds", "6C91225C31DCB094.dds", "6D16EB74289BFDEB.dds", "7BEB2430EEDA6DB3.dds", "82E6908652A274A5.dds", "85EC7F221BAA9B80.dds", "8D56A2A1130FBF7F.dds", "93BBA90B91481EF3.dds", "BAAE7913CA40F4BB.dds", "CA78ED331E52F69C.dds", "EA2776265AF6E3B7.dds", "F418E9A2947E3F22.dds", "F6E1C08D3C9C1491.dds", "F9BBB5040E79B576.dds", "FE083329CAD537B9.dds"
};
static const char *page_hashes[24]={
    "c59fff53636e6dee82d91dc897589df9d9fd79c24da6504f60af9add33f2569e", "5e8cc8d59508bf16ee60e880b1f92d38a4b2d8d94619375208cfab834c6f2715", "e3475b275e4097541915eb161a5088acf68073b84f5d0e2074a57028807b1b9b", "631caf2d8b703005624cc2fb54032416eb3b2b292708ba110b480107fc8d8841", "9c49529f081645a8a90a8eb7f6147292217d07d99d33d49c2577f1653644c83b", "d19ee40eb461b39e9fd2c337f534a4a804fe4bd66498cbee3bd29fb22da653ce", "20f407b5bf9d52ac536a79914ba5858dc436a9dffa7d7bce717fa56906911cbc", "263d79711929e870ee44e633e62ecb12567983141e31e241d617a5b77828ca56", "7b2f0839232e943c851c986c4f76dc34de5172fba2abaa0749b66dd48400c635", "676e73a964f7c90c3a80d8958cc399842b1fc94a9340cfb696785bbf0d55d641", "3f7efb09746cc6fcdcc592472b1b9d084f172c7126d6769dfa4f20ec53df1855", "e5ea568621feb69501a578ac2f2cf16a57f3108be8ad3f4a0564bc9b2a34822d", "a41e6aa8ed1c3235704fcad11c8ebde84e1156765d4efacbc0328f80cf617730", "510c2a97a6eb61f90b9738273d2a9412eb7bfde3b9af7119c2ddd3766c7a2b41", "0aa7cb603ea9e45813b0e62142d4fbae9d4ba9540aebaae713d52068d40e7bca", "26d8f1595459b7ffc5bd83f3bd2a9db78dfa62a13bf16d76f7522ce61e777508", "486e599c79e781ccc1da3d3a6afbbe8add1eb96720981ea97daea3d7a06c8e9a", "09f55d46d2522149ea688d945112e67bfc668eb358fca668acbb22a441ef6987", "52d8817a5f1c1d73dd3c8c099776a64f17e5f2a9c09522a07148005cf05528f0", "59b5ed3d002890fe35330132236b974906f9aa97d3a31031c57280f22bf4f66d", "d211b7bdca41d4a4a733b1a7c63bbf78ef28b32559fc02c33c2bba5fb88839a8", "b42463a02c91f50e5fe591be86bb6de5f7637a4f4f8a1b01741ee58507e7a4c0", "a184a970fe3eec9762028fac5161f3be30ee8f1ab8d15b7c84722551d3be1075", "c394c34ac8cec47744d85a608e73477cc146b9f1fb745eb93bae15cdead14d00"
};
static const unsigned char *page_starts[24]={ gpu_page_0_start, gpu_page_1_start, gpu_page_2_start, gpu_page_3_start, gpu_page_4_start, gpu_page_5_start, gpu_page_6_start, gpu_page_7_start, gpu_page_8_start, gpu_page_9_start, gpu_page_10_start, gpu_page_11_start, gpu_page_12_start, gpu_page_13_start, gpu_page_14_start, gpu_page_15_start, gpu_page_16_start, gpu_page_17_start, gpu_page_18_start, gpu_page_19_start, gpu_page_20_start, gpu_page_21_start, gpu_page_22_start, gpu_page_23_start };
static const unsigned char *page_ends[24]={ gpu_page_0_end, gpu_page_1_end, gpu_page_2_end, gpu_page_3_end, gpu_page_4_end, gpu_page_5_end, gpu_page_6_end, gpu_page_7_end, gpu_page_8_end, gpu_page_9_end, gpu_page_10_end, gpu_page_11_end, gpu_page_12_end, gpu_page_13_end, gpu_page_14_end, gpu_page_15_end, gpu_page_16_end, gpu_page_17_end, gpu_page_18_end, gpu_page_19_end, gpu_page_20_end, gpu_page_21_end, gpu_page_22_end, gpu_page_23_end };

#define CHECK(x, label) do { if(!(x)){fprintf(stderr,"FAIL: %s\n",label);fails++;}else{printf("PASS: %s\n",label);} } while(0)
static int fails;
static int hash_bytes(const unsigned char *data,size_t len,char out[65]){
 BCRYPT_ALG_HANDLE a=NULL;BCRYPT_HASH_HANDLE h=NULL;DWORD z=0,n=0;PUCHAR obj=NULL;unsigned char digest[32];
 int ok=BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,NULL,0)>=0;
 if(ok)ok=BCryptGetProperty(a,BCRYPT_OBJECT_LENGTH,(PUCHAR)&z,sizeof(z),&n,0)>=0;
 if(ok)obj=HeapAlloc(GetProcessHeap(),0,z);if(ok&&!obj)ok=0;
 if(ok)ok=BCryptCreateHash(a,&h,obj,z,NULL,0,0)>=0;
 if(ok&&len)ok=BCryptHashData(h,(PUCHAR)data,(ULONG)len,0)>=0;
 if(ok)ok=BCryptFinishHash(h,digest,32,0)>=0;
 if(h)BCryptDestroyHash(h);if(obj)HeapFree(GetProcessHeap(),0,obj);if(a)BCryptCloseAlgorithmProvider(a,0);
 if(!ok)return 0;static const char d[]="0123456789abcdef";for(int i=0;i<32;i++){out[2*i]=d[digest[i]>>4];out[2*i+1]=d[digest[i]&15];}out[64]=0;return 1;
}
static int write_bytes(const char*p,const void*b,size_t n){HANDLE f=CreateFileA(p,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return 0;DWORD w=0;int ok=WriteFile(f,b,(DWORD)n,&w,NULL)&&w==n;CloseHandle(f);return ok;}
static int read_bytes(const char*p,unsigned char*b,size_t cap,size_t*n){HANDLE f=CreateFileA(p,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return 0;DWORD got=0;int ok=ReadFile(f,b,(DWORD)cap,&got,NULL);CloseHandle(f);*n=got;return ok;}
static int has_file(const char*p){DWORD a=GetFileAttributesA(p);return a!=INVALID_FILE_ATTRIBUTES&&!(a&FILE_ATTRIBUTE_DIRECTORY);}
static int has_dir(const char*p){DWORD a=GetFileAttributesA(p);return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY);}
static int path(char*out,size_t cap,const char*root,const char*leaf){int n=snprintf(out,cap,"%s\\%s",root,leaf);return n>0&&(size_t)n<cap;}
static void cleanup(const char *dir){char search[MAX_PATH*4];WIN32_FIND_DATAA fd;if(snprintf(search,sizeof(search),"%s\\*",dir)>=(int)sizeof(search))return;HANDLE h=FindFirstFileA(search,&fd);if(h!=INVALID_HANDLE_VALUE){do{if(!strcmp(fd.cFileName,".")||!strcmp(fd.cFileName,".."))continue;char child[MAX_PATH*4];if(!path(child,sizeof(child),dir,fd.cFileName))continue;if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){cleanup(child);RemoveDirectoryA(child);}else DeleteFileA(child);}while(FindNextFileA(h,&fd));FindClose(h);}RemoveDirectoryA(dir);}
static int fill_profile(HogsProfileFile*f){
 static char rel[30][96];
 for(int i=0;i<24;i++){snprintf(rel[i],sizeof(rel[i]),"TT\\inject\\%s",page_names[i]);f[i]=(HogsProfileFile){rel[i],page_starts[i],(size_t)(page_ends[i]-page_starts[i]),page_hashes[i]};}
 const unsigned char *a[6]={gpu_ddraw_start,gpu_d3dimm_start,gpu_dgvoodoo_conf_start,gpu_toolkit_asi_start,gpu_toolkit_license_start,gpu_dgvoodoo_license_start};
 const unsigned char *b[6]={gpu_ddraw_end,gpu_d3dimm_end,gpu_dgvoodoo_conf_end,gpu_toolkit_asi_end,gpu_toolkit_license_end,gpu_dgvoodoo_license_end};
 const char*n[6]={"DDraw.dll","D3DImm.dll","dgVoodoo.conf","TextureToolkit-x86.asi","LICENSE-TextureToolkit.txt","DgVoodoo-Redistribution.txt"};
 for(int i=0;i<6;i++){snprintf(rel[24+i],sizeof(rel[24+i]),"%s",n[i]);f[24+i]=(HogsProfileFile){rel[24+i],a[i],(size_t)(b[i]-a[i]),NULL};}return 1;
}
static int make_archives(const char*root,char*guard,size_t cap,size_t*guard_len){
 size_t at=0;const char*magic="HOGSORG1\n";memcpy(guard,magic,9);at=9;
 for(unsigned i=0;i<140;i++){char name[32],file[MAX_PATH*4],content[64],hex[65],line[128];snprintf(name,sizeof(name),"a%03u.bin",i);if(!path(file,sizeof(file),root,name))return 0;int cn=snprintf(content,sizeof(content),"fixture-archive-%03u\n",i);if(!write_bytes(file,content,(size_t)cn)||!hash_bytes((unsigned char*)content,(size_t)cn,hex))return 0;int ln=snprintf(line,sizeof(line),"%s\t%s\n",name,hex);if(at+(size_t)ln>=cap)return 0;memcpy(guard+at,line,(size_t)ln);at+=(size_t)ln;}
 *guard_len=at;return 1;
}
static int install(const char*root,const unsigned char*g,size_t gn,HogsProfileFile*f,char*msg){return hogs_gpu_profile_install(root,g,gn,f,30,msg,512);}
int main(void){
 char temp[MAX_PATH],root[MAX_PATH*4],msg[512],exe[MAX_PATH];DWORD n=GetTempPathA(sizeof(temp),temp);if(!n||n>=sizeof(temp)){puts("FAIL: temp path");return 2;}
 snprintf(root,sizeof(root),"%sHogsGPUProfileFixture-%lu",temp,(unsigned long)GetCurrentProcessId());if(!CreateDirectoryA(root,NULL)){puts("FAIL: create fixture root");return 2;}
 unsigned char *guard=HeapAlloc(GetProcessHeap(),0,32768);size_t guard_len=0;HogsProfileFile files[30];fill_profile(files);
 if(!guard||!make_archives(root,(char*)guard,32768,&guard_len)){puts("FAIL: synthetic archive setup");cleanup(root);return 2;}
 HogsProfileFile corrupt[30];memcpy(corrupt,files,sizeof(files));corrupt[29].sha256="0000000000000000000000000000000000000000000000000000000000000000";
 int ok=install(root,guard,guard_len,corrupt,msg);path(exe,sizeof(exe),root,"TT");CHECK(!ok&&!has_dir(exe),"bad payload rejected before creating profile directories");
 path(exe,sizeof(exe),root,"a000.bin");write_bytes(exe,"modified\n",9);ok=install(root,guard,guard_len,files,msg);path(exe,sizeof(exe),root,"TT");CHECK(!ok&&!has_dir(exe),"modified archive rejected before profile mutation");
 path(exe,sizeof(exe),root,"a000.bin");write_bytes(exe,"fixture-archive-000\n",20);
 path(exe,sizeof(exe),root,"DDraw.dll");write_bytes(exe,"original ddraw",14);
 ok=install(root,guard,guard_len,files,msg);CHECK(ok,"complete HD profile install");
 path(exe,sizeof(exe),root,"TT\\inject\\29EF8CF01DE8C0C1.dds");CHECK(has_file(exe),"texture page created");
 char marker[MAX_PATH*4];path(marker,sizeof(marker),root,".Hogs-GPU-HD-Profile.txt");unsigned char marker_backup[32768];size_t marker_n=0;CHECK(read_bytes(marker,marker_backup,sizeof(marker_backup),&marker_n),"install receipt written");
 ok=install(root,guard,guard_len,files,msg);CHECK(!ok,"duplicate install rejected");
 const char bad_marker[]="HOGSGPU1\n.Hogs-GPU-HD-backup-test\n0\n0\n31\n..\\warhogs_.exe\t0\t-\t0000000000000000000000000000000000000000000000000000000000000000\n";write_bytes(marker,bad_marker,sizeof(bad_marker)-1);
 ok=hogs_gpu_profile_restore(root,NULL,0,msg,sizeof(msg));path(exe,sizeof(exe),root,"TT\\inject\\29EF8CF01DE8C0C1.dds");CHECK(!ok&&has_file(exe),"foreign traversal receipt rejected without deleting profile data");
 write_bytes(marker,marker_backup,marker_n);
 ok=hogs_gpu_profile_restore(root,NULL,0,msg,sizeof(msg));CHECK(ok,"restore succeeds");
 path(exe,sizeof(exe),root,"TT\\inject\\29EF8CF01DE8C0C1.dds");CHECK(!has_file(exe),"new texture page removed on restore");
 path(exe,sizeof(exe),root,"DDraw.dll");unsigned char buf[128];size_t got=0;CHECK(read_bytes(exe,buf,sizeof(buf),&got)&&got==14&&!memcmp(buf,"original ddraw",14),"pre-existing DLL restored byte-for-byte");
 ok=install(root,guard,guard_len,files,msg);CHECK(ok,"reinstall after restore succeeds");ok=hogs_gpu_profile_restore(root,NULL,0,msg,sizeof(msg));CHECK(ok,"second restore succeeds");
 if(guard)HeapFree(GetProcessHeap(),0,guard);cleanup(root);printf("RESULT: %s (%d failures)\n",fails?"FAIL":"PASS",fails);return fails?1:0;
}
