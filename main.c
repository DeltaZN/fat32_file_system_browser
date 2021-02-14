#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include "fat32_lib.c"

char *const SHELL_MODE = "shell";
char *const MOUNTS_LIST_MODE = "mounts";

typedef struct {
    char disk_name[256];
    char major_minor_numbers[32];
    char fs_type[32];
    char fs_version[32];
} disk_info;

int run_mounts_mode();

int startsWith(const char *str, const char *pre) {
    size_t lenpre = strlen(pre),
            lenstr = strlen(str);
    return lenstr < lenpre ? 0 : memcmp(pre, str, lenpre) == 0;
}

char * get_line(void) {
    char * line = malloc(100), * linep = line;
    size_t lenmax = 100, len = lenmax;
    int c;

    if(line == NULL)
        return NULL;

    for(;;) {
        c = fgetc(stdin);
        if(c == EOF)
            break;

        if(--len == 0) {
            len = lenmax;
            char * linen = realloc(linep, lenmax *= 2);

            if(linen == NULL) {
                free(linep);
                return NULL;
            }
            line = linen + (line - linep);
            linep = linen;
        }

        if((*line++ = c) == '\n')
            break;
    }
    *line = '\0';
    return linep;
}

void run_second_mode() {
    partition_value_t *pValue = open_partition("sdc1");
    while (1) {
        char *line = get_line();
        if (startsWith(line, "ls")) {
            dir_value_t *pDirValue = read_dir(pValue->active_cluster, pValue);
            print_dir(pDirValue);
        } else if (startsWith(line, "cd")) {
            char *arg = calloc(1, 256);
            const char delim[] = " ";
            char *ptr = strtok(line, delim);
            ptr = strtok(NULL, delim);

            while(ptr != NULL)
            {
                strcat(arg, ptr);
                ptr = strtok(NULL, delim);
                strcat(arg, " ");
            }
            for (int i = 0;; i++) {
                if (arg[i] == '\n') {
                    arg[i] = 0;
                }
                if (arg[i] == 0) {
                    break;
                }
            }
            change_dir(pValue, arg);
        } else if (startsWith(line, "exit")) {
            break;
        }
    }
    close_partition(pValue);
}

int main(int argc, char **argv) {
    run_second_mode();

//    run_mounts_mode();
//    if (argc < 2) {
//        printf("Please, specify program mode (shell/mounts)\n");
//        return 0;
//    } else {
//        char *mode = argv[1];
//        if (!strcmp(SHELL_MODE, mode)) {
//            printf("Starting program in shell mode...\n");
//        } else if (!strcmp(MOUNTS_LIST_MODE, mode)) {
//            printf("Starting program in mounts mode...\n");
//            return run_mounts_mode();
//        }
//    }
//    printf("Unknown mode, terminating program!\n");
    return -1;
}

void read_devices_properties(disk_info *device) {
    char const *FS_TYPE_PROPERTY = "E:ID_FS_TYPE";
    char const *FS_VERSION_PROPERTY = "E:ID_FS_VERSION";

    char udev_data_file_name[32] = "/run/udev/data/b";
    strcat(udev_data_file_name, device->major_minor_numbers);

    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(udev_data_file_name, "rb");
    if (fp == NULL)
        exit(EXIT_FAILURE);

    char tmp[32];

    while ((read = getline(&line, &len, fp)) != -1) {
        if (startsWith(line, FS_TYPE_PROPERTY)) {
            int property_len = strlen(FS_TYPE_PROPERTY) + 1;
            strncpy(tmp, &line[property_len], read - property_len);
            for (int i = 0; i < 32; ++i) {
                if (tmp[i] == '\n') {
                    tmp[i] = 0;
                    break;
                }
            }
            strcpy(device->fs_type, tmp);
        } else if (startsWith(line, FS_VERSION_PROPERTY)) {
            int property_len = strlen(FS_VERSION_PROPERTY) + 1;
            strncpy(tmp, &line[property_len], read - property_len);
            for (int i = 0; i < 32; ++i) {
                if (tmp[i] == '\n') {
                    tmp[i] = 0;
                    break;
                }
            }
            strcpy(device->fs_version, tmp);
        }
    }

    fclose(fp);
    if (line)
        free(line);
}

int run_mounts_mode() {
    const int MAX_DISKS = 256;
    disk_info *disks = (disk_info *) malloc(MAX_DISKS * sizeof(disk_info));
    disk_info *partitions = (disk_info *) malloc(MAX_DISKS * sizeof(disk_info));

    DIR *d = opendir("/sys/block");
    DIR *dd;
    struct dirent *sys_bloc_dir;
    struct dirent *sys_bloc_disk_dir;
    int disks_counter = 0;
    int parts_counter = 0;
    if (d) {
        while ((sys_bloc_dir = readdir(d)) != NULL) {
            if (startsWith(sys_bloc_dir->d_name, "sd")) {
                char disk_dir_name[256] = "/sys/block/";
                strcat(disk_dir_name, sys_bloc_dir->d_name);
                char disk_dev_file_name[256];
                strcpy(disk_dev_file_name, disk_dir_name);
                strcat(disk_dev_file_name, "/dev");

                FILE *f = fopen(disk_dev_file_name, "rb");
                char *buffer = 0;
                long length;

                if (f) {
                    fseek(f, 0, SEEK_END);
                    length = ftell(f);
                    fseek(f, 0, SEEK_SET);
                    // to be sure we have zero byte at the end
                    buffer = malloc(length + 1);
                    if (buffer) {
                        fread(buffer, 1, length, f);
                        for (int i = 0; i < length; ++i) {
                            if (buffer[i] == 0) {
                                break;
                            }
                            if (buffer[i] == '\n') {
                                buffer[i] = 0;
                            }
                        }
                    }
                    fclose(f);
                }

                strcpy(disks[disks_counter].disk_name, sys_bloc_dir->d_name);
                strcpy(disks[disks_counter].major_minor_numbers, buffer);
                read_devices_properties(&disks[disks_counter]);
                disks_counter++;

                dd = opendir(disk_dir_name);
                while ((sys_bloc_disk_dir = readdir(dd)) != NULL) {
                    if (startsWith(sys_bloc_disk_dir->d_name, "sd")) {
                        char part_dir_name[256];
                        strcpy(part_dir_name, disk_dir_name);
                        strcat(part_dir_name, "/");
                        strcat(part_dir_name, sys_bloc_disk_dir->d_name);
                        char part_dev_file_name[256];
                        strcpy(part_dev_file_name, part_dir_name);
                        strcat(part_dev_file_name, "/dev");

                        FILE *ff = fopen(part_dev_file_name, "rb");
                        char *bufferr;
                        long lengthh;

                        if (ff) {
                            fseek(ff, 0, SEEK_END);
                            lengthh = ftell(ff);
                            fseek(ff, 0, SEEK_SET);
                            // to be sure we have zero byte at the end
                            bufferr = malloc(lengthh + 1);
                            if (bufferr) {
                                fread(bufferr, 1, lengthh, ff);
                                for (int i = 0; i < lengthh; ++i) {
                                    if (bufferr[i] == 0) {
                                        break;
                                    }
                                    if (bufferr[i] == '\n') {
                                        bufferr[i] = 0;
                                    }
                                }
                            }
                            fclose(ff);
                        }

                        strcpy(partitions[parts_counter].disk_name, sys_bloc_disk_dir->d_name);
                        strcpy(partitions[parts_counter].major_minor_numbers, bufferr);
                        read_devices_properties(&partitions[parts_counter]);
                        parts_counter++;
                        free(bufferr);
                    }
                }
                free(buffer);
                closedir(dd);
            }
        }
        closedir(d);
    }
    for (int i = 0; i < disks_counter; ++i) {
        printf("disk name: %s \ndisk mm: %s\nfs type: %s\nfs ver: %s\n",
               disks[i].disk_name, disks[i].major_minor_numbers, disks[i].fs_type, disks[i].fs_version);
    }
    for (int i = 0; i < parts_counter; ++i) {
        printf("partition name: %s \npartition mm: %s\nfs type: %s\nfs ver: %s\n",
               partitions[i].disk_name, partitions[i].major_minor_numbers, partitions[i].fs_type, partitions[i].fs_version);
    }
    return 0;
}
