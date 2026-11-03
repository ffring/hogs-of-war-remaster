#include <assert.h>
#include "render_layout.h"
int main(int argc,char **argv) {
    HMMenuState m={0};HmrLayout l;
    assert(!hmr_layout(&m,0,&l));
    assert(hm_menu_open(&m,1,12,300,1,1));
    for (int selected=0;selected<5;++selected) {
        m.selected=selected;
        for(int error=0;error<3;++error) {
            assert(hmr_layout(&m,error,&l));
            assert(l.count==(error?13u:12u));
            for (unsigned i=0;i<l.count;++i) {
                HmrLine *line=&l.lines[i];
                assert(line->x>=205 && line->x<=680 && line->y>=175 && line->y<=652);
                assert(strlen(line->text)<HMR_LINE_BYTES);
                for (const char *p=line->text;*p;++p) assert(*p>=32 && *p<127 && *p!='/');
            }
        }
    }
    m.selected=0;assert(hmr_layout(&m,2,&l));
    if (argc==5) {
        const char *names[]={argv[2],argv[3],argv[4]};
        for (int selected=0;selected<4;++selected) {
            assert(hmr_options_layout(selected,argv[1],names,&l));
            assert(l.count==7 && strstr(l.lines[4].text,"MSIWS"));
        }
        assert(l.lines[4].selected && l.lines[4].text[0]=='>');
    } else assert(argc==1);
    printf("{\"lines\":[");
    for (unsigned i=0;i<l.count;++i) printf("%s{\"x\":%d,\"y\":%d,\"selected\":%d,\"text\":\"%s\"}",
        i?",":"",l.lines[i].x,l.lines[i].y,l.lines[i].selected,l.lines[i].text);
    puts("]}");return 0;
}
