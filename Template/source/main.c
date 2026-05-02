#include <3ds.h>
#include <stdio.h>
#include <string.h>

char hb_path[256] __attribute__((section(".data"))) = "sdmc:/3ds/placeholder.3dsx";

int main(int argc, char* argv[]) {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    if (!aptMainLoop()) return 0;

    // Write the target path to the nextload file
    // Rosalina hbmenu reads this on next launch
    FILE *f = fopen("sdmc:/3ds/nextload", "w");
    if (f) {
        fwrite(hb_path, 1, strlen(hb_path), f);
        fclose(f);
    }

    gfxExit();
    return 0;
}