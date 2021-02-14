#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
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

char *get_line(void) {
    char *line = malloc(100), *linep = line;
    size_t lenmax = 100, len = lenmax;
    int c;

    if (line == NULL)
        return NULL;

    for (;;) {
        c = fgetc(stdin);
        if (c == EOF)
            break;

        if (--len == 0) {
            len = lenmax;
            char *linen = realloc(linep, lenmax *= 2);

            if (linen == NULL) {
                free(linep);
                return NULL;
            }
            line = linen + (line - linep);
            linep = linen;
        }

        if ((*line++ = c) == '\n')
            break;
    }
    *line = '\0';
    return linep;
}

void remove_ending_symbol(char *str, char sym) {
    for (int i = 0;; i++) {
        if (str[i] == sym)
            str[i] = 0;
        if (str[i] == 0)
            break;
    }
}

void remove_until(char *str, char sym) {
    int len = strlen(str);
    for (int i = len; i >= 0; i--) {
        if (str[i] != sym)
            str[i] = 0;
        else {
            str[i] = 0;
            break;
        }
    }
}

void print_dir(dir_value_t *pValue) {
    while (pValue != NULL) {
        if (pValue->type == 'd') {
            printf("DIR %s %d\n", pValue->filename, pValue->first_cluster);
        } else {
            printf("FILE %s (%d bytes)\n", pValue->filename, pValue->size);
        }
        pValue = pValue->next;
    }
}

void copy_file(partition_value_t *part, char *dest, dir_value_t *file) {
    if (file->type != 'f') {
        printf("Not a file\n");
        return;
    }
    char *buf = malloc(part->cluster_size);
    unsigned int fat_record = file->first_cluster;
    int fd = open(dest, O_RDWR | O_APPEND | O_CREAT, 0777);
    unsigned int size = file->size < part->cluster_size ? file->size : part->cluster_size;
    while (fat_record < 0x0FFFFFF7) {
        fat_record = read_file_cluster(part, fat_record, buf);
        write(fd, buf, size);
    }
    free(buf);
    close(fd);
}

void copy_dir(partition_value_t *part, char *dest, dir_value_t *file) {
    if (file->type != 'd') {
        printf("Not a dir\n");
        return;
    }
    struct stat dir = {0};
    if (stat(dest, &dir) == -1) {
        mkdir(dest, 0777);
    }
    dir_value_t *dir_val = read_dir(file->first_cluster, part);
    while (dir_val != NULL) {
        if (strcmp(dir_val->filename, ".") && strcmp(dir_val->filename, "..")) {
            char *path = calloc(1, 512);
            strcat(path, dest);
            strcat(path, "/");
            strcat(path, dir_val->filename);
            if (dir_val->type == 'd') {
                copy_dir(part, path, dir_val);
            } else {
                copy_file(part, path, dir_val);
            }
            free(path);
        }
        dir_val = dir_val->next;
    }
}

char *get_arg(char *ptr) {
    const char delim[] = " ";
    char *arg = calloc(1, 256);
    ptr = strtok(NULL, delim);

    while (ptr != NULL) {
        strcat(arg, ptr);
        ptr = strtok(NULL, delim);
        strcat(arg, " ");
    }
    remove_ending_symbol(arg, '\n');
    return arg;
}

void run_shell_mode(const char *part, const char *extract_path) {
    partition_value_t *pValue = open_partition(part);
    if (pValue) {
        printf("FAT32 supported.\n");
        char *current_dir = calloc(1, 512);
        strcat(current_dir, "/");
        strcat(current_dir, part);
        char *ptr;
        const char delim[] = " ";
        while (1) {
            printf("%s$ ", current_dir);
            ptr = strtok(get_line(), delim);
            remove_ending_symbol(ptr, '\n');
            if (!strcmp(ptr, "ls")) {
                dir_value_t *pDirValue = read_dir(pValue->active_cluster, pValue);
                print_dir(pDirValue);
            } else if (!strcmp(ptr, "cd")) {
                char *arg = get_arg(ptr);
                if (change_dir(pValue, arg)) {
                    if (!strcmp(arg, "..")) {
                        remove_until(current_dir, '/');
                    } else if (!strcmp(".", arg)) {
                        // do nothing
                    } else {
                        strcat(current_dir, "/");
                        strcat(current_dir, arg);
                    }
                    printf("%s\n", arg);
                } else {
                    printf("Dir doesn't exist.\n");
                }
                free(arg);
            } else if (!strcmp(ptr, "exit")) {
                break;
            } else if (!strcmp(ptr, "pwd")) {
                printf("%s\n", current_dir);
            } else if (!strcmp(ptr, "cp")) {
                char *arg = get_arg(ptr);
                dir_value_t *pDirValue = read_dir(pValue->active_cluster, pValue);
                while (pDirValue != NULL) {
                    if (!strcmp(pDirValue->filename, arg)) {
                        char *filename = calloc(1, 512);
                        strcpy(filename, extract_path);
                        strcat(filename, pDirValue->filename);

                        if (pDirValue->type == 'd') {
                            copy_dir(pValue, filename, pDirValue);
                        } else {
                            copy_file(pValue, filename, pDirValue);
                        }
                        free(filename);
                        printf("copied\n");
                        break;
                    }
                    pDirValue = pDirValue->next;
                }
                free(arg);
            } else {
                printf("Unknown command\n");
            }
        }
        close_partition(pValue);
    } else {
        printf("FAT32 not supported.\n");
    }
}

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
                printf("usage: ./lab1 shell sda1 /home/gosha/fat32copy/");
                return 0;
            }
            if (check_directory(argv[3])) {
                run_shell_mode(argv[2], argv[3]);
            } else {
                printf("Directory doesn't exist.");
                return 0;
            }
        } else if (!strcmp(MOUNTS_LIST_MODE, mode)) {
            printf("Starting program in mounts mode...\n");
            return run_mounts_mode();
        }
    }
    printf("Unknown mode, terminating program!\n");
    return 0;
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
               partitions[i].disk_name, partitions[i].major_minor_numbers, partitions[i].fs_type,
               partitions[i].fs_version);
    }
    return 0;
}
