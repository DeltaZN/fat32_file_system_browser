#include <stdio.h>
#include <string.h>

char* const SHELL_MODE = "shell";
char* const MOUNTS_LIST_MODE = "mounts";

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Please, specify program mode (shell/mounts)\n");
        return 0;
    } else {
        char *mode = argv[1];
        if (!strcmp(SHELL_MODE, mode)) {
            printf("Starting program in shell mode...\n");
        } else if (!strcmp(MOUNTS_LIST_MODE, mode)) {
            printf("Starting program in mounts mode...\n");
        }
    }
    printf("Unknown mode, terminating program!\n");
    return -1;
}
