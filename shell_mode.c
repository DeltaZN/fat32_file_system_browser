
void print_dir(struct dir_value *pValue) {
    while (pValue != NULL) {
        if (pValue->type == 'd') {
            printf("DIR %s %d\n", pValue->filename, pValue->first_cluster);
        } else {
            printf("FILE %s (%d bytes)\n", pValue->filename, pValue->size);
        }
        pValue = pValue->next;
    }
}

void copy_file(struct partition_value *part, char *dest, struct dir_value *file) {
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

void copy_dir(struct partition_value *part, char *dest, struct dir_value *file) {
    if (file->type != 'd') {
        printf("Not a dir\n");
        return;
    }
    struct stat dir = {0};
    if (stat(dest, &dir) == -1) {
        mkdir(dest, 0777);
    }
    struct dir_value *dir_val = read_dir(file->first_cluster, part);
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

void print_help() {
    printf("cd [arg] - change working directory\n");
    printf("pwd - print working directory full name\n");
    printf("cp [arg] - copy dir or file to mounted device\n");
    printf("ls - show working directory elements\n");
    printf("exit - terminate program\n");
    printf("help - print help\n");
}

void run_shell_mode(const char *part) {
    struct partition_value *partition = open_partition(part);
    if (partition) {
        printf("FAT32 supported.\n");
        char *current_dir = calloc(1, 512);
        strcat(current_dir, "/");
        strcat(current_dir, part);
        char *line;
        int exit = 0;
        while (!exit) {
            char *args[3];
            args[0] = calloc(1, 256);
            args[1] = calloc(1, 256);
            args[2] = calloc(1, 256);
            printf("%s$ ", current_dir);
            line = get_line();
            remove_ending_symbol(line, '\n');
            parse(line, args);
            if (!strcmp(args[0], "ls")) {
                struct dir_value *dir_value = read_dir(partition->active_cluster, partition);
                print_dir(dir_value);
                destroy_dir_value(dir_value);
            } else if (!strcmp(args[0], "cd")) {
                if (change_dir(partition, (unsigned char*)args[1])) {
                    if (!strcmp(args[1], "..")) {
                        remove_until(current_dir, '/');
                    } else if (!strcmp(".", args[1])) {
                        // do nothing
                    } else {
                        strcat(current_dir, "/");
                        strcat(current_dir, args[1]);
                    }
                    printf("%s\n", args[1]);
                } else {
                    printf("Dir doesn't exist.\n");
                }
            } else if (!strcmp(args[0], "exit")) {
                exit = 1;
            } else if (!strcmp(args[0], "pwd")) {
                printf("%s\n", current_dir);
            } else if (!strcmp(args[0], "cp")) {
                struct dir_value *dir_value = read_dir(partition->active_cluster, partition);
                char copied = 0;
                while (dir_value != NULL) {
                    if (!strcmp((char*)dir_value->filename, args[1])) {
                        if (check_directory(args[2])) {
                            char filename[256] = {0};
                            strcpy(filename, args[2]);
                            size_t str_len = strlen(args[2]);
                            if (filename[str_len - 1] != '/') {
                                strcat(filename, "/");
                            }
                            strcat(filename, (char *) dir_value->filename);

                            if (dir_value->type == 'd') {
                                copy_dir(partition, filename, dir_value);
                            } else {
                                copy_file(partition, filename, dir_value);
                            }
                            copied = 1;
                            break;
                        } else {
                            printf("Directory doesn't exists\n");
                        }
                    }
                    dir_value = dir_value->next;
                }
                if (copied) {
                    printf("copied\n");
                } else {
                    printf("Dir/file not found\n");
                }
                destroy_dir_value(dir_value);
            } else if (!strcmp(args[0], "help")) {
                print_help();
            } else {
                printf("Unknown command\n");
            }
            free(args[0]);
            free(args[1]);
            free(args[2]);
        }
        close_partition(partition);
    } else {
        printf("FAT32 not supported.\n");
    }
}

