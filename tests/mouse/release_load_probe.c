#include <windows.h>
#include <stdio.h>
int main(int argc, char **argv) {
    if (argc!=2) return 2;
    HMODULE m=LoadLibraryA(argv[1]);
    if (!m) { printf("FAIL LoadLibrary error=%lu\n",(unsigned long)GetLastError());return 1; }
    puts("PASS release ASI imports and DllMain loaded in console fixture; no game or mouse input");
    FreeLibrary(m);return 0;
}
