#ifndef HOGS_RENDER_LAYOUT_H
#define HOGS_RENDER_LAYOUT_H
#include <stdio.h>
#include <string.h>
#include "native_render.h"
enum { HMR_MAX_LINES=16, HMR_LINE_BYTES=80 };
typedef struct { int x,y,selected; char text[HMR_LINE_BYTES]; } HmrLine;
typedef struct { unsigned count; HmrLine lines[HMR_MAX_LINES]; } HmrLayout;
/* Decoded glyph stream for the installed Russian font atlas, not Unicode. */
static inline void hmr_line(HmrLayout *layout,int x,int y,int selected,const char *text) {
    if (layout->count>=HMR_MAX_LINES) return;
    HmrLine *line=&layout->lines[layout->count++];
    line->x=x;line->y=y;line->selected=selected;
    snprintf(line->text,sizeof(line->text),"%s",text);
}
static inline int hmr_options_layout(int selected,const char *title,
                                      const char *const names[3],HmrLayout *layout) {
    if (!layout || !title || !names || !names[0] || !names[1] || !names[2] ||
        selected<0 || selected>3) return 0;
    memset(layout,0,sizeof(*layout));
    hmr_line(layout,205,175,1,title);
    for (int row=0;row<4;++row) {
        char text[HMR_LINE_BYTES];
        snprintf(text,sizeof(text),"%s %.74s",selected==row?">":" ",row==3?"MSIWS":names[row]);
        hmr_line(layout,205,270+row*62,selected==row,text);
    }
    hmr_line(layout,205,610,0,"CTPEVKN: BSIFOP");
    hmr_line(layout,205,652,0,"HA3AD");
    return 1;
}
static inline int hmr_layout(const HMMenuState *m,int error,HmrLayout *layout) {
    static const char *names[5]={"UOBOPOT","UPNLEV","NHBEPCNR X","NHBEPCNR Y","COXPAHNTS"};
    if (!m || !layout || !m->active || m->selected<0 || m->selected>=5 ||
        m->turn_units<1 || m->turn_units>24 || m->aim_percent<25 || m->aim_percent>1000)
        return 0;
    memset(layout,0,sizeof(*layout));
    hmr_line(layout,205,175,1,"MSIWS");
    for (int row=0;row<5;++row) {
        int y=270+row*62,chosen=m->selected==row;
        char text[80];
        snprintf(text,sizeof(text),"%s %s",chosen?">":" ",names[row]);
        hmr_line(layout,205,y,chosen,text);
        if (row<4) {
            if (row==0) snprintf(text,sizeof(text),"%d%%",m->turn_units*25);
            else if (row==1) snprintf(text,sizeof(text),"%d%%",m->aim_percent);
            else snprintf(text,sizeof(text),"%s",(row==2?m->invert_x:m->invert_y)?"BKV":"BSIKV");
            hmr_line(layout,680,y,chosen,text);
        }
    }
    hmr_line(layout,205,610,0,"CTPEVKN: HACTPONKA");
    hmr_line(layout,205,652,0,"HA3AD: FE3 COXPAHEHNR");
    if (error) hmr_line(layout,205,565,1,error==1?"HE YDAVOCS UPO4NTATS":"HE YDAVOCS COXPAHNTS");
    return 1;
}
#endif
