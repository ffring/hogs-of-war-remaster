#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "mac_focus_gate.h"

int main(int argc, char **argv) {
    DWORD start=GetTickCount();
    unsigned duration=argc>1?(unsigned)atoi(argv[1]):7000;
    setvbuf(stdout,NULL,_IONBF,0);
    puts("READY");
    do {
        int allow=hm_mac_focus_gate();
        printf("S %lu %d %d\n",(unsigned long)(GetTickCount()-start),allow,hm_mac_focus_status_code());
        Sleep(25);
    } while ((DWORD)(GetTickCount()-start)<duration);
    return 0;
}
