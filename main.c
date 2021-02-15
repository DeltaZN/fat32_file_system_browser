#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "fat32_lib.c"
#include "utils.c"
#include "list_mode.c"
#include "shell_mode.c"

char *const SHELL_MODE = "shell";
char *const MOUNTS_LIST_MODE = "list";

int check_directory(const char *path) {
    DIR *dir = opendir(path);
    if (dir) return 1;
    else return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Please, specify program mode (shell/mounts)\n");
        return 0;
    } else {
        char *mode = argv[1];
        if (!strcmp(SHELL_MODE, mode)) {
            if (argc < 4) {
                printf("You have to specify target device with FAT32 fs, and existing directory to copy files\n");
                printf("usage: ./lab1 shell sda1 /home/gosha/fat32copy/\n");
                return 0;
            }
            if (check_directory(argv[3])) {
                printf("Starting program in shell mode...\n");
                run_shell_mode(argv[2], argv[3]);
            } else {
                printf("Directory doesn't exist.\n");
                return 0;
            }
        } else if (!strcmp(MOUNTS_LIST_MODE, mode)) {
            printf("Starting program in mounts mode...\n");
            return run_mounts_mode();
        } else {
            printf("Unknown mode, terminating program!\n");
        }
    }
    return 0;
}
