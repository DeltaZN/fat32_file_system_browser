#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <argp.h>
#include "unistd.h"
#include "fat32_lib.c"
#include "utils.c"
#include "list_mode.c"
#include "shell_mode.c"

char *const SHELL_MODE = "shell";
char *const LIST_MODE = "list";

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Please, specify program mode (%s/%s)\n", SHELL_MODE, LIST_MODE);
        return 0;
    } else {
        char *mode = argv[1];
        if (!strcmp(SHELL_MODE, mode)) {
            if (argc < 3) {
                printf("You have to specify target device with FAT32 fs\n");
                printf("usage: ./lab1 shell sda1\n");
                return 0;
            }
            printf("Starting program in shell mode...\n");
            run_shell_mode(argv[2]);
        } else if (!strcmp(LIST_MODE, mode)) {
            printf("Starting program in mounts mode...\n");
            run_list_mode();
        } else {
            printf("Unknown mode, terminating program!\n");
        }
    }
    return 0;
}
