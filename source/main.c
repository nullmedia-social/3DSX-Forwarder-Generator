#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define TITLE_ID_OFFSET 14864
#define MAX_FILES       100
#define PLACEHOLDER     "sdmc:/3ds/placeholder.3dsx"
#define DUMMY_TITLE_ID  0x000400000F7FFF00ULL

char file_list[MAX_FILES][256];
int file_count = 0;
int selected_idx = 0;

long find_bytes(unsigned char *buffer, long size, unsigned char *needle, long needle_len) {
    for (long i = 0; i < size - needle_len; i++) {
        if (memcmp(buffer + i, needle, needle_len) == 0) return i;
    }
    return -1;
}

long find_placeholder(unsigned char *buffer, long size) {
    return find_bytes(buffer, size, (unsigned char *)PLACEHOLDER, strlen(PLACEHOLDER));
}

long find_title_id(unsigned char *buffer, long size) {
    unsigned char dummy[8];
    uint64_t dummy_id = DUMMY_TITLE_ID;
    memcpy(dummy, &dummy_id, 8);
    return find_bytes(buffer, size, dummy, 8);
}

void scan_3ds_dir() {
    DIR *d = opendir("sdmc:/3ds");
    struct dirent *dir;
    file_count = 0;
    if (d) {
        while ((dir = readdir(d)) != NULL && file_count < MAX_FILES) {
            if (strstr(dir->d_name, ".3dsx")) {
                strncpy(file_list[file_count], dir->d_name, 255);
                file_list[file_count][255] = '\0';
                file_count++;
            }
        }
        closedir(d);
    }
}

void print_file_list() {
    consoleClear();
    printf("\x1b[1;34m== 3DSX Forwarder Generator ==\x1b[0m\n\n");

    if (file_count == 0) {
        printf("No .3dsx files found in sdmc:/3ds/\n");
    } else {
        for (int i = 0; i < file_count; i++) {
            if (i == selected_idx)
                printf("\x1b[32;1m> %s\x1b[0m\n", file_list[i]);
            else
                printf("  %s\n", file_list[i]);
        }
    }

    printf("\n\x1b[37mDPad: Navigate  A: Generate  START: Quit\x1b[0m\n");
}

void create_forwarder(const char *filename) {
    mkdir("sdmc:/fwd_tool", 0777);

    char target_path[300];
    snprintf(target_path, sizeof(target_path), "sdmc:/3ds/%s", filename);

    FILE *template_fp = fopen("sdmc:/fwd_tool/template.cia", "rb");
    if (!template_fp) {
        consoleClear();
        printf("\x1b[31;1mError: sdmc:/fwd_tool/template.cia missing!\x1b[0m\n");
        printf("\nPress B to go back.\n");
        return;
    }

    fseek(template_fp, 0, SEEK_END);
    long fsize = ftell(template_fp);
    fseek(template_fp, 0, SEEK_SET);

    unsigned char *buffer = malloc(fsize);
    if (!buffer) {
        fclose(template_fp);
        consoleClear();
        printf("\x1b[31;1mError: Out of memory!\x1b[0m\n");
        printf("\nPress B to go back.\n");
        return;
    }

    fread(buffer, 1, fsize, template_fp);
    fclose(template_fp);

    // Find and patch the placeholder path
    long path_off = find_placeholder(buffer, fsize);
    if (path_off == -1) {
        consoleClear();
        printf("\x1b[31;1mError: Placeholder string not found in template!\x1b[0m\n");
        printf("Is your template.cia built correctly?\n");
        printf("\nPress B to go back.\n");
        free(buffer);
        return;
    }

    // Find and patch the Title ID dynamically
    long tid_off = find_title_id(buffer, fsize);
    if (tid_off == -1) {
        consoleClear();
        printf("\x1b[31;1mError: Dummy Title ID not found in template!\x1b[0m\n");
        printf("Is DUMMY_TITLE_ID correct?\n");
        printf("\nPress B to go back.\n");
        free(buffer);
        return;
    }

    // Generate a random unique ID using system tick for better entropy
    srand((unsigned int)svcGetSystemTick());
    uint32_t random_id = 0xF0000 + (rand() % 0x0FFFF);
    uint64_t full_title_id = 0x0004000000000000ULL | ((uint64_t)random_id << 8);

    // Patch path — zero out the full field first to avoid leftover bytes
    memset(buffer + path_off, 0, 256);
    memcpy(buffer + path_off, target_path, strlen(target_path));

    // Patch Title ID
    memcpy(buffer + tid_off, &full_title_id, 8);

    // Save output CIA
    char out_path[128];
    snprintf(out_path, sizeof(out_path), "sdmc:/fwd_tool/%05lX.cia", (unsigned long)random_id);

    FILE *output_fp = fopen(out_path, "wb");
    if (output_fp) {
        fwrite(buffer, 1, fsize, output_fp);
        fclose(output_fp);
        consoleClear();
        printf("\x1b[32;1mSuccess!\x1b[0m\n\n");
        printf("Created:\n%s\n\n", out_path);
        printf("Install this CIA with FBI.\n");
    } else {
        consoleClear();
        printf("\x1b[31;1mError: Could not write output file!\x1b[0m\n");
        printf("Check SD card write permissions.\n");
    }

    printf("\nPress B to go back.\n");
    free(buffer);
}

int main(int argc, char *argv[]) {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    if (!aptMainLoop()) return 0;

    scan_3ds_dir();
    print_file_list();

    bool in_result_screen = false;

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (in_result_screen) {
            if (kDown & KEY_B) {
                in_result_screen = false;
                print_file_list();
            }
        } else {
            if (kDown & KEY_DOWN) {
                if (file_count > 0) {
                    selected_idx = (selected_idx + 1) % file_count;
                    print_file_list();
                }
            }
            if (kDown & KEY_UP) {
                if (file_count > 0) {
                    selected_idx = (selected_idx - 1 + file_count) % file_count;
                    print_file_list();
                }
            }
            if (kDown & KEY_A && file_count > 0) {
                create_forwarder(file_list[selected_idx]);
                in_result_screen = true;
            }
            if (kDown & KEY_START) break;
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}