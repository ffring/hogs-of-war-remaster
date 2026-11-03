#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <ddraw.h>
#include <stdint.h>
#include <string.h>
#include "render_layout.h"

enum { HMR_FONT_BYTES=0x649C, HMR_QUEUE_CAPACITY=5000 };
static unsigned char hmr_font[HMR_FONT_BYTES];
typedef void *(__attribute__((thiscall)) *HmrPosition)(void*,int,int);
typedef void *(__attribute__((thiscall)) *HmrReset)(void*);
typedef void *(__attribute__((thiscall)) *HmrDraw)(void*,const char*,int,int);
typedef const char *(__attribute__((thiscall)) *HmrLookup)(void*,int);
static int hmr_memory(const void *p,size_t n,int write) {
    uintptr_t at=(uintptr_t)p,end=at+n;
    if (!at || end<at) return 0;
    while (at<end) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery((void*)at,&mbi,sizeof(mbi)) || mbi.State!=MEM_COMMIT ||
            (mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return 0;
        DWORD prot=mbi.Protect&0xff;
        if (write && prot!=PAGE_READWRITE && prot!=PAGE_EXECUTE_READWRITE &&
            prot!=PAGE_WRITECOPY && prot!=PAGE_EXECUTE_WRITECOPY) return 0;
        uintptr_t next=(uintptr_t)mbi.BaseAddress+mbi.RegionSize;
        if (next<=at) return 0;
        at=next;
    }
    return 1;
}
static int hmr_executable(const void *p) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!p || !VirtualQuery(p,&mbi,sizeof(mbi)) || mbi.State!=MEM_COMMIT ||
        (mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return 0;
    DWORD prot=mbi.Protect&0xff;
    return prot==PAGE_EXECUTE || prot==PAGE_EXECUTE_READ ||
        prot==PAGE_EXECUTE_READWRITE || prot==PAGE_EXECUTE_WRITECOPY;
}
static int hmr_code(void) {
    static const BYTE draw[]={0x8b,0x44,0x24,0x08,0x53,0x55,0x56,0x83,0xf8,0xff};
    static const BYTE pos[]={0x8b,0x54,0x24,0x08,0x8b,0xc1,0x8b,0x4c,0x24,0x04};
    static const BYTE reset[]={0x8b,0xc1,0x33,0xc9,0x89,0x48,0x54,0x89,0x48,0x50,0xc3};
    BYTE *base=(BYTE*)GetModuleHandleA(NULL);
    if ((uintptr_t)base!=0x400000 || !hmr_memory(base,0x1000,0)) return 0;
    IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER*)base;
    if (dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<64 || dos->e_lfanew>0xe00) return 0;
    IMAGE_NT_HEADERS32 *nt=(IMAGE_NT_HEADERS32*)(base+dos->e_lfanew);
    if (nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.TimeDateStamp!=0x3a069f75 ||
        nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 || nt->OptionalHeader.Magic!=0x10b ||
        nt->OptionalHeader.ImageBase!=0x400000 || nt->OptionalHeader.SizeOfImage!=0x15e000) return 0;
    return hmr_memory((void*)0x430ed0,sizeof(draw),0) && !memcmp((void*)0x430ed0,draw,sizeof(draw)) &&
        hmr_memory((void*)0x431b30,sizeof(pos),0) && !memcmp((void*)0x431b30,pos,sizeof(pos)) &&
        hmr_memory((void*)0x431b10,sizeof(reset),0) && !memcmp((void*)0x431b10,reset,sizeof(reset));
}
static DWORD hmr_get(const BYTE *p,unsigned offset) { DWORD v;memcpy(&v,p+offset,4);return v; }
static void hmr_set(BYTE *p,unsigned offset,DWORD v) { memcpy(p+offset,&v,4); }
static int hmr_context(BYTE **font,LPDIRECTDRAWSURFACE7 *back) {
    if (!hmr_code() || !hmr_memory((void*)0x51f120,4,0) || !*(DWORD*)0x51f120 ||
        !hmr_memory((void*)0x51ba54,4,0) || !hmr_memory((void*)0x52083c,4,0) ||
        !hmr_memory((void*)0x511760,4,1)) return 0;
    *font=*(BYTE**)0x51ba54;
    if (!hmr_memory(*font,HMR_FONT_BYTES,0)) return 0;
    DWORD count=*(DWORD*)0x511760;
    if (count>HMR_QUEUE_CAPACITY || !hmr_memory((void*)0x4e0a20,HMR_QUEUE_CAPACITY*40,1)) return 0;
    BYTE *game=*(BYTE**)0x52083c;
    if (!hmr_memory(game,12,0)) return 0;
    *back=*(LPDIRECTDRAWSURFACE7*)(game+8);
    if (!hmr_memory(*back,sizeof(void*),0) ||
        !hmr_memory((*back)->lpVtbl,sizeof(IDirectDrawSurface7Vtbl),0) ||
        !hmr_executable((void*)(*back)->lpVtbl->Blt) ||
        !hmr_executable((void*)(*back)->lpVtbl->GetSurfaceDesc)) return 0;
    DDSURFACEDESC2 desc;memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);
    if (FAILED(IDirectDrawSurface7_GetSurfaceDesc(*back,&desc)) || desc.dwWidth!=640 || desc.dwHeight!=480) return 0;
    /* Only initialized native bitmap font objects, whose glyph table fits the
       known atlas, are accepted. Borrowed surface pointers are never released. */
    if (!hmr_get(*font,0x6c) || hmr_get(*font,0x70)>4096 || hmr_get(*font,0x74)>4096) return 0;
    return 1;
}
int hmr_ready(void) { BYTE *font;LPDIRECTDRAWSURFACE7 back;return hmr_context(&font,&back); }
static int hmr_render(const HmrLayout *layout,BYTE *font,LPDIRECTDRAWSURFACE7 back) {
    RECT rect={0,0,640,480};DDBLTFX fx;memset(&fx,0,sizeof(fx));fx.dwSize=sizeof(fx);fx.dwFillColor=0;
    if (FAILED(IDirectDrawSurface7_Blt(back,&rect,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fx))) return 0;
    memcpy(hmr_font,font,sizeof(hmr_font));
    hmr_set(hmr_font,0x10,1);hmr_set(hmr_font,0x14,3);
    hmr_set(hmr_font,0x1c,0);hmr_set(hmr_font,0x20,0);hmr_set(hmr_font,0x24,0);
    hmr_set(hmr_font,0x3c,0);hmr_set(hmr_font,0x58,0);hmr_set(hmr_font,0x5c,0);
    hmr_set(hmr_font,0x64,0);hmr_set(hmr_font,0x68,0);
    /* Original queued frontend text is superseded before the native reverse
       sprite flush. The game itself resets this plain POD queue every frame. */
    *(DWORD*)0x511760=0;
    for (unsigned i=0;i<layout->count;++i) {
        const HmrLine *line=&layout->lines[i];
        DWORD color=line->selected?128:70;
        hmr_set(hmr_font,0x2c,color);hmr_set(hmr_font,0x30,color);hmr_set(hmr_font,0x34,color);
        ((HmrReset)(uintptr_t)0x431b10)(hmr_font);
        ((HmrPosition)(uintptr_t)0x431b30)(hmr_font,line->x,line->y);
        ((HmrDraw)(uintptr_t)0x430ed0)(hmr_font,line->text,-1,-1);
    }
    return 1;
}
int hmr_draw_menu(const HMMenuState *menu,int error) {
    HmrLayout layout;BYTE *font;LPDIRECTDRAWSURFACE7 back;
    if (!hmr_layout(menu,error,&layout) || !hmr_context(&font,&back)) return 0;
    return hmr_render(&layout,font,back);
}
static int hmr_original_label(BYTE *font,int id,char out[HMR_LINE_BYTES]) {
    const BYTE *encoded=(const BYTE*)((HmrLookup)(uintptr_t)0x431680)(font,id);
    for (unsigned i=0;i<HMR_LINE_BYTES-1;++i) {
        if (!hmr_memory(encoded+i,1,0)) return 0;
        if (!encoded[i]) { out[i]=0;return i!=0; }
        unsigned decoded=(unsigned)encoded[i]+0x1f;
        if (decoded<32 || decoded>135) return 0;
        out[i]=(char)decoded;
    }
    return 0;
}
int hmr_draw_options(int selected) {
    HmrLayout layout;BYTE *font;LPDIRECTDRAWSURFACE7 back;
    char title[HMR_LINE_BYTES],labels[3][HMR_LINE_BYTES];
    const char *names[3]={labels[0],labels[1],labels[2]};
    if (!hmr_context(&font,&back) ||
        !hmr_original_label(font,17,title)) return 0;
    for (int i=0;i<3;++i) if (!hmr_original_label(font,21+i,labels[i])) return 0;
    if (!hmr_options_layout(selected,title,names,&layout)) return 0;
    return hmr_render(&layout,font,back);
}
