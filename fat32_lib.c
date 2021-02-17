#include <unistd.h>
#include <fcntl.h>

struct fat_BS {
    unsigned char boot_jmp[3];
    unsigned char oem_name[8];
    unsigned short bytes_per_sector;
    unsigned char sectors_per_cluster;
    unsigned short reserved_sector_count;
    unsigned char table_count;
    unsigned short root_entry_count;
    unsigned short total_sectors_16;
    unsigned char media_type;
    unsigned short table_size_16;
    unsigned short sectors_per_track;
    unsigned short head_side_count;
    unsigned int hidden_sector_count;
    unsigned int total_sectors_32;

    unsigned int table_size_32;
    unsigned short extended_flags;
    unsigned short fat_version;
    unsigned int root_cluster;
    unsigned short fat_info;
    unsigned short backup_BS_sector;
    unsigned char reserved_0[12];
    unsigned char drive_number;
    unsigned char reserved_1;
    unsigned char boot_signature;
    unsigned int volume_id;
    unsigned char volume_label[11];
    unsigned char fat_type_label[8];

}__attribute__((packed));

struct fs_info {
    unsigned int lead_signature;
    unsigned char reserved_0[480];
    unsigned int another_signature;
    unsigned int last_free_cluster;
    unsigned int available;
    unsigned char reserved_1[12];
    unsigned int tail_signature;
}__attribute__((packed));

struct dir_entry {
    unsigned char file_name[8];
    unsigned char extension[3];
    unsigned char attributes;
    unsigned char reserved;
    unsigned char crt_time_0;
    unsigned short crt_time_1;
    unsigned short crt_date;
    unsigned short last_date_access;
    unsigned short high_cluster_num;
    unsigned short last_mod_time;
    unsigned short last_mod_date;
    unsigned short low_cluster_num;
    unsigned int file_size;
}__attribute__((packed));

struct long_filename {
    unsigned char order;
    unsigned char _2_byte_chars[10];
    unsigned char attribute;
    unsigned char long_entry_type;
    unsigned char checksum;
    unsigned char _2_byte_chars_next[12];
    unsigned short always_zero;
    unsigned char _2_byte_chars_last[4];
}__attribute__((packed));

struct dir_value {
    unsigned char *filename;
    unsigned char type;
    unsigned int first_cluster;
    unsigned int size;
    void *next;
}__attribute__((packed));

struct partition_value {
    int device_fd;
    unsigned int cluster_size;
    unsigned int first_data_sector;
    unsigned int active_cluster;
    struct fat_BS *fat_boot;
    struct fs_info *fs_info;
}__attribute__((packed));

unsigned int
get_fat_table_value(unsigned int active_cluster, unsigned int first_fat_sector, unsigned int sector_size, int fd) {
    unsigned char *FAT_table = malloc(sector_size);
    unsigned int fat_offset = active_cluster * 4;
    unsigned int fat_sector = first_fat_sector + (fat_offset / sector_size);
    unsigned int ent_offset = fat_offset % sector_size;
    pread(fd, FAT_table, sector_size, fat_sector * sector_size);
    unsigned int table_value = *(unsigned int *) &FAT_table[ent_offset] & 0x0FFFFFFF;
    free(FAT_table);
    return table_value;
}

unsigned int get_first_sector(struct partition_value *part, unsigned int cluster) {
    return ((cluster - 2) * part->fat_boot->sectors_per_cluster) + part->first_data_sector;
}

unsigned int read_file_cluster(struct partition_value *part, unsigned int cluster, char *buf) {
    unsigned int first_sector = get_first_sector(part, cluster);
    pread(part->device_fd, buf, part->cluster_size, first_sector * part->fat_boot->bytes_per_sector);
    return get_fat_table_value(cluster, part->fat_boot->reserved_sector_count, part->fat_boot->bytes_per_sector, part->device_fd);
}

struct dir_value *init_dir_value(struct dir_entry *entry, unsigned char *filename) {
    struct dir_value *dir_val = calloc(1, sizeof(struct dir_value));
    dir_val->filename = calloc(1, 256);
    strcpy((char*)dir_val->filename, (char*)filename);
    dir_val->size = entry->file_size;
    dir_val->type = ((entry->attributes & 0x20) == 0x20) ? 'f' : 'd';
    dir_val->first_cluster = entry->high_cluster_num << 4;
    dir_val->first_cluster =
            dir_val->first_cluster + (entry->low_cluster_num & 0xFFFF);
    if (dir_val->first_cluster == 0)
        dir_val->first_cluster = 2;

    return dir_val;
}

void destroy_dir_value(struct dir_value *dir_val) {
    if (dir_val) {
        free(dir_val->filename);
        struct dir_value *next = dir_val;
        while (next != NULL) {
            struct dir_value *current = next;
            next = current->next;
            free(current);
        }
    }
}

struct dir_value *read_dir(unsigned int first_cluster, struct partition_value *value) {
    unsigned int cluster_size = value->cluster_size;
    unsigned int sector_size = value->fat_boot->bytes_per_sector;
    unsigned int current_cluster = first_cluster;
    int fd = value->device_fd;
    unsigned int first_sector = get_first_sector(value, current_cluster);
    struct dir_entry *buf = calloc(1, cluster_size);
    pread(fd, buf, cluster_size, first_sector * sector_size);
    struct dir_value *first_dir_value = NULL;
    struct dir_value *prev_dir_value = NULL;
    struct dir_value *current_dir_value = NULL;

    char *order_bitmap = calloc(1, 32);
    char *order[32];
    unsigned int long_name_counter = 0;
    char end_dir_reached = 0;
    int j = 0;
    while (!end_dir_reached) {
        struct dir_entry *entry = &buf[j++];
        if ((cluster_size / sizeof(struct dir_entry)) <= (j)) {
            // cluster limit reached
            unsigned int fat_record = get_fat_table_value(current_cluster, value->fat_boot->reserved_sector_count,
                                                          sector_size, value->device_fd);
            if (fat_record >= 0x0FFFFFF7) {
                // chain end reached or bad cluster...
                end_dir_reached = 1;
            } else {
                current_cluster = fat_record;
                free(buf);
                unsigned int current_sector = get_first_sector(value, current_cluster);
                buf = calloc(1, cluster_size);
                j = 0;
                pread(fd, buf, cluster_size, current_sector * sector_size);
                continue;
            }
        }
        if (entry->file_name[0] == 0) {
            // dir end
            end_dir_reached = 1;
        } else if (entry->file_name[0] == 0xE5) {
            // unused entry - skip
            continue;
        } else if (entry->attributes == 0x0F) {
            struct long_filename *filename = (struct long_filename *) entry;
            // maximum order value == 0x1F
            int current_order = filename->order & 0x001F;
            order[current_order] = calloc(1, 13);
            order_bitmap[current_order] = 1;
            char *current_buf = order[current_order];
            int buf_offset = 0;
            for (int i = 0; i < 10; i += 2) {
                current_buf[buf_offset++] = (char)filename->_2_byte_chars[i];
            }
            for (int i = 0; i < 12; i += 2) {
                current_buf[buf_offset++] = (char)filename->_2_byte_chars_next[i];
            }
            for (int i = 0; i < 4; i += 2) {
                current_buf[buf_offset++] = (char)filename->_2_byte_chars_last[i];
            }
            long_name_counter++;
        } else if ((entry->attributes & 0x10) == 0x10 || (entry->attributes & 0x20) == 0x20) {
            if (!long_name_counter) {
                char tmp_name[9];
                char tmp_ext[4];
                strncpy(tmp_name, (char*)entry->file_name, 8);
                strncpy(tmp_ext, (char*)entry->extension, 3);
                for (int i = 7; i >= 0; --i) {
                    if (tmp_name[i] == 32) {
                        tmp_name[i] = 0;
                    } else {
                        break;
                    }
                }
                for (int i = 2; i >= 0; --i) {
                    if (tmp_ext[i] == 32) {
                        tmp_ext[i] = 0;
                    } else {
                        break;
                    }
                }

                char *filename = calloc(1, 11);
                strcpy(filename, tmp_name);
                if (strlen(tmp_ext)) {
                    strcat(filename, ".");
                    strcat(filename, tmp_ext);
                }

                current_dir_value = init_dir_value(entry, (unsigned char*)filename);

                if (first_dir_value == NULL)
                    first_dir_value = current_dir_value;
                if (prev_dir_value != NULL)
                    prev_dir_value->next = current_dir_value;
                prev_dir_value = current_dir_value;
            } else {
                unsigned char *tmp_str = calloc(1, long_name_counter * 13);
                for (int i = 0; i < 32; ++i) {
                    if (order_bitmap[i] == 1) {
                        strcat((char*)tmp_str, order[i]);
                        order_bitmap[i] = 0;
                        free(order[i]);
                    }
                }

                long_name_counter = 0;
                current_dir_value = init_dir_value(entry, tmp_str);

                if (first_dir_value == NULL)
                    first_dir_value = current_dir_value;
                if (prev_dir_value != NULL)
                    prev_dir_value->next = current_dir_value;
                prev_dir_value = current_dir_value;
            }
        }
    }

    free(order_bitmap);
    free(buf);
    return first_dir_value;
}

int change_dir(struct partition_value *value, const unsigned char *dir_name) {
    struct dir_value *dir_val = read_dir(value->active_cluster, value);
    while (dir_val != NULL) {
        if (dir_val->type == 'd' && strcmp((char*)dir_name, (char*)dir_val->filename) == 0) {
            value->active_cluster = dir_val->first_cluster;
            destroy_dir_value(dir_val);
            return 1;
        }
        dir_val = dir_val->next;
    }
    destroy_dir_value(dir_val);
    return 0;
}

struct partition_value *open_partition(const char *partition) {
    char dev[256] = "/dev/";
    strcat(dev, partition);
    int fd = open(dev, O_RDONLY, 00666);
    struct fat_BS *fat_boot;
    if (fd != -1) {
        fat_boot = malloc(sizeof(struct fat_BS));
        pread(fd, fat_boot, sizeof(struct fat_BS), 0);
        unsigned int total_sectors = fat_boot->total_sectors_32;
        unsigned int fat_size = (fat_boot->table_size_16 == 0) ? fat_boot->table_size_32 : fat_boot->table_size_16;
        unsigned int root_dir_sectors =
                ((fat_boot->root_entry_count * 32) + (fat_boot->bytes_per_sector - 1)) / fat_boot->bytes_per_sector;
        unsigned int first_data_sector =
                fat_boot->reserved_sector_count + (fat_boot->table_count * fat_size) + root_dir_sectors;
        unsigned int data_sectors = total_sectors -
                                    (fat_boot->reserved_sector_count + (fat_boot->table_count * fat_size) +
                                     root_dir_sectors);
        unsigned int total_clusters = data_sectors / fat_boot->sectors_per_cluster;

        struct fs_info *fs = malloc(sizeof(struct fs_info));
        pread(fd, fs, sizeof(struct fs_info), fat_boot->fat_info * fat_boot->bytes_per_sector);

        if (total_clusters >= 65525 && total_clusters < 268435445
            && fs->lead_signature == 0x41615252
            && fs->another_signature == 0x61417272
            && fs->tail_signature == 0xAA550000) {
            // filesystem supported
        } else {
            // filesystem not supported
            return NULL;
        }

        unsigned int cluster_size = fat_boot->bytes_per_sector * fat_boot->sectors_per_cluster;

        struct partition_value *part = malloc(sizeof(struct partition_value));
        part->cluster_size = cluster_size;
        part->device_fd = fd;
        part->fat_boot = fat_boot;
        part->fs_info = fs;
        part->first_data_sector = first_data_sector;
        part->active_cluster = fat_boot->root_cluster;

        return part;
    }
    return NULL;
}

void close_partition(struct partition_value *part) {
    free(part->fat_boot);
    free(part->fs_info);
    close(part->device_fd);
    free(part);
}