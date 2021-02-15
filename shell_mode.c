
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
        if (strcmp((char*)dir_val->filename, ".") != 0 && strcmp((char*)dir_val->filename, "..") != 0) {
            char *path = calloc(1, 512);
            strcat(path, dest);
            append_path_part(path, (char*)dir_val->filename);
            if (dir_val->type == 'd') {
                copy_dir(part, path, dir_val);
            } else {
                copy_file(part, path, dir_val);
            }
            free(path);
        }
        dir_val = dir_val->next;
    }
    destroy_dir_value(dir_val);
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

void print_help() {
    printf("cd [arg] - change working directory\n");
    printf("pwd - print working directory full name\n");
    printf("cp [arg] - copy dir or file to mounted device\n");
    printf("ls - show working directory elements\n");
    printf("exit - terminate program\n");
    printf("help - print help\n");
}

void run_shell_mode(const char *part, const char *extract_path) {
    partition_value_t *partition = open_partition(part);
    if (partition) {
        printf("FAT32 supported.\n");
        char *current_dir = calloc(1, 512);
        strcat(current_dir, "/");
        strcat(current_dir, part);
        char *ptr;
        const char delim[] = " ";
        int exit = 0;
        while (!exit) {
            printf("%s$ ", current_dir);
            ptr = strtok(get_line(), delim);
            remove_ending_symbol(ptr, '\n');
            if (!strcmp(ptr, "ls")) {
                dir_value_t *dir_value = read_dir(partition->active_cluster, partition);
                print_dir(dir_value);
                destroy_dir_value(dir_value);
            } else if (!strcmp(ptr, "cd")) {
                char *arg = get_arg(ptr);
                if (change_dir(partition, (unsigned char*)arg)) {
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
                exit = 1;
            } else if (!strcmp(ptr, "pwd")) {
                printf("%s\n", current_dir);
            } else if (!strcmp(ptr, "cp")) {
                char *arg = get_arg(ptr);
                dir_value_t *dir_value = read_dir(partition->active_cluster, partition);
                while (dir_value != NULL) {
                    if (!strcmp((char*)dir_value->filename, arg)) {
                        char *filename = calloc(1, 512);
                        strcpy(filename, extract_path);
                        strcat(filename, (char*)dir_value->filename);

                        if (dir_value->type == 'd') {
                            copy_dir(partition, filename, dir_value);
                        } else {
                            copy_file(partition, filename, dir_value);
                        }
                        free(filename);
                        printf("copied\n");
                        break;
                    }
                    dir_value = dir_value->next;
                }
                destroy_dir_value(dir_value);
                free(arg);
            } else if (!strcmp(ptr, "help")) {
                print_help();
            } else {
                printf("Unknown command\n");
            }
        }
        close_partition(partition);
    } else {
        printf("FAT32 not supported.\n");
    }
}

