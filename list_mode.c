//
// Created by gosha on 15.02.2021.
//

typedef struct {
    char disk_name[256];
    char major_minor_numbers[32];
    char fs_type[32];
    char fs_version[32];
} disk_info;

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
        return;

    char property_value[32];

    while ((read = getline(&line, &len, fp)) != -1) {
        if (startsWith(line, FS_TYPE_PROPERTY)) {
            int property_len = (int)strlen(FS_TYPE_PROPERTY) + 1;
            strncpy(property_value, &line[property_len], read - property_len);
            remove_ending_symbol(property_value, '\n');
            strcpy(device->fs_type, property_value);
        } else if (startsWith(line, FS_VERSION_PROPERTY)) {
            int property_len = (int)strlen(FS_VERSION_PROPERTY) + 1;
            strncpy(property_value, &line[property_len], read - property_len);
            remove_ending_symbol(property_value, '\n');
            strcpy(device->fs_version, property_value);
        }
    }

    fclose(fp);
    if (line)
        free(line);
}

void add_disk_partition_info(disk_info *info, struct dirent *sys_block_child, const char* disk_dir_name) {
    char disk_dev_file_name[256] = {0};
    strcpy(disk_dev_file_name, disk_dir_name);
    strcat(disk_dev_file_name, "/dev");

    FILE *disk_dev_file = fopen(disk_dev_file_name, "rb");
    char *buffer = 0;
    long length;

    if (disk_dev_file) {
        fseek(disk_dev_file, 0, SEEK_END);
        length = ftell(disk_dev_file);
        fseek(disk_dev_file, 0, SEEK_SET);
        buffer = calloc(1, length);
        fread(buffer, 1, length, disk_dev_file);
        remove_ending_symbol(buffer, '\n');
        fclose(disk_dev_file);
    }

    strcpy(info->disk_name, sys_block_child->d_name);
    strcpy(info->major_minor_numbers, buffer);
    read_devices_properties(info);
    free(buffer);
}

int run_mounts_mode() {
    const int MAX_DISKS = 256;
    const char *sys_block_path = "/sys/block/";
    disk_info *disks = (disk_info *) malloc(MAX_DISKS * sizeof(disk_info));
    disk_info *partitions = (disk_info *) malloc(MAX_DISKS * sizeof(disk_info));

    DIR *sys_block_dir = opendir(sys_block_path);
    DIR *sys_block_disk_dir;
    struct dirent *sys_block_child;
    struct dirent *sys_block_disk_child;
    int disks_counter = 0;
    int parts_counter = 0;
    if (sys_block_dir) {
        while ((sys_block_child = readdir(sys_block_dir)) != NULL) {
            if (startsWith(sys_block_child->d_name, "sd")) {
                char disk_dir_name[256] = {0};
                strcat(disk_dir_name, sys_block_path);
                strcat(disk_dir_name, sys_block_child->d_name);
                add_disk_partition_info(&disks[disks_counter], sys_block_child, disk_dir_name);
                disks_counter++;

                sys_block_disk_dir = opendir(disk_dir_name);
                while ((sys_block_disk_child = readdir(sys_block_disk_dir)) != NULL) {
                    if (startsWith(sys_block_disk_child->d_name, "sd")) {
                        char part_dir_name[256] = {0};
                        strcpy(part_dir_name, disk_dir_name);
                        append_path_part(part_dir_name, sys_block_disk_child->d_name);
                        add_disk_partition_info(&partitions[parts_counter], sys_block_disk_child, part_dir_name);
                        parts_counter++;
                    }
                }
                closedir(sys_block_disk_dir);
            }
        }
        closedir(sys_block_dir);
    }
    for (int i = 0; i < disks_counter; ++i) {
        printf("disk name: %s\n", disks[i].disk_name);
    }
    for (int i = 0; i < parts_counter; ++i) {
        printf("partition name: %s (%s %s)\n",
               partitions[i].disk_name, partitions[i].fs_type, partitions[i].fs_version);
    }
    return 0;
}