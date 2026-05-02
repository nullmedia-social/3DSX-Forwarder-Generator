#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h> // For mkdir

#define PATH_OFFSET     75008
#define TITLE_ID_OFFSET 14864
#define MAX_FILES       100

char file_list[MAX_FILES][256];
int file_count = 0;
int selected_idx = 0;

void scan_3ds_dir() {
    DIR *d = opendir("sdmc:/3ds");
    struct dirent *dir;
    file_count = 0;
    if (d) {
        while ((dir = readdir(d)) != NULL && file_count < MAX_FILES) {
            if (strstr(dir->d_name, ".3dsx")) {
                strncpy(file_list[file_count], dir->d_name, 256);
                file_count++;
            }
        }
        closedir(d);
    }
}

void create_forwarder(const char* filename) {
    mkdir("sdmc:/fwd_tool", 0777);

    char target_path[300];
    snprintf(target_path, sizeof(target_path), "sdmc:/3ds/%s", filename);

    FILE *template_fp = fopen("sdmc:/fwd_tool/template.cia", "rb");
    if (!template_fp) {
        printf("\x1b[31;1mError: template.cia missing!\x1b[0m\n");
        return;
    }

    // 1. Generate a random Unique ID (3 bytes)
    srand(time(NULL));
    uint32_t random_id = 0xF0000 + (rand() % 0x0FFFF);
    
    // 2. Construct the full 64-bit Title ID: 00040000 + UniqueID + 00
    uint64_t full_title_id = 0x0004000000000000ULL | ((uint64_t)random_id << 8);

    char out_path[128];
    snprintf(out_path, sizeof(out_path), "sdmc:/fwd_tool/%05X.cia", random_id);
    
    FILE *output_fp = fopen(out_path, "wb");
    if (!output_fp) {
        printf("\x1b[31;1mError: Could not create output file!\x1b[0m\n");
        fclose(template_fp);
        return;
    }

    // Copy template to new file
    char buffer[BUFSIZ];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), template_fp)) > 0) {
        fwrite(buffer, 1, n, output_fp);
    }

    // Patch Path
    fseek(output_fp, PATH_OFFSET, SEEK_SET);
    fwrite(target_path, 1, strlen(target_path) + 1, output_fp);

    // Patch Title ID
    fseek(output_fp, TITLE_ID_OFFSET, SEEK_SET);
    fwrite(&full_title_id, 8, 1, output_fp); // Write all 8 bytes

    fclose(template_fp);
    fclose(output_fp);
    
    printf("\x1b[32;1mSuccess! Created:\x1b[0m\n%s\n", out_path);
}

int main(int argc, char* argv[]) {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    printf("\x1b[1;34mForwarder Generator Tool\x1b[0m\n");
    printf("Press START to exit\n\n");

    scan_3ds_dir();

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        consoleClear();
        printf("\x1b[1;37mSelect a .3dsx to forward:\x1b[0m\n\n");

        if (file_count == 0) {
            printf("No files found in /3ds/\n");
        } else {
            for (int i = 0; i < file_count; i++) {
                if (i == selected_idx) printf("\x1b[47;30m > %s \x1b[0m\n", file_list[i]);
                else printf("   %s \n", file_list[i]);
            }
        }

        if (kDown & KEY_UP && file_count > 0) selected_idx = (selected_idx - 1 + file_count) % file_count;
        if (kDown & KEY_DOWN && file_count > 0) selected_idx = (selected_idx + 1) % file_count;

        if (kDown & KEY_A && file_count > 0) {
            create_forwarder(file_list[selected_idx]);
            printf("\nPress B to go back.");
            while (aptMainLoop()) {
                hidScanInput();
                if (hidKeysDown() & KEY_B) break;
                gfxFlushBuffers();
                gfxSwapBuffers();
                gspWaitForVBlank();
            }
        }

        if (kDown & KEY_START) break;

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}