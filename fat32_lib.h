//
// Created by georgii on 24.02.2021.
//

#ifndef LAB1_FAT32_LIB_H
#define LAB1_FAT32_LIB_H

struct fat_BS {
    u_int8_t boot_jmp[3];
    u_char oem_name[8];
    u_int16_t bytes_per_sector;
    u_int8_t sectors_per_cluster;
    u_int16_t reserved_sector_count;
    u_int8_t table_count;
    u_int16_t root_entry_count;
    u_int16_t total_sectors_16;
    u_int8_t media_type;
    u_int16_t table_size_16;
    u_int16_t sectors_per_track;
    u_int16_t head_side_count;
    u_int32_t hidden_sector_count;
    u_int32_t total_sectors_32;

    u_int32_t table_size_32;
    u_int16_t extended_flags;
    u_int16_t fat_version;
    u_int32_t root_cluster;
    u_int16_t fat_info;
    u_int16_t backup_BS_sector;
    u_int8_t reserved_0[12];
    u_int8_t drive_number;
    u_int8_t reserved_1;
    u_int8_t boot_signature;
    u_int32_t volume_id;
    u_char volume_label[11];
    u_char fat_type_label[8];
}__attribute__((packed));

struct fs_info {
    u_int32_t lead_signature;
    u_int8_t reserved_0[480];
    u_int32_t another_signature;
    u_int32_t last_free_cluster;
    u_int32_t available;
    u_int8_t reserved_1[12];
    u_int32_t tail_signature;
}__attribute__((packed));

struct dir_value {
    u_char *filename;
    u_char type;
    u_int32_t first_cluster;
    u_int32_t size;
    void *next;
};

struct partition_value {
    int32_t device_fd;
    u_int32_t cluster_size;
    u_int32_t first_data_sector;
    u_int32_t active_cluster;
    struct fat_BS *fat_boot;
    struct fs_info *fs_info;
};

u_int32_t read_file_cluster(struct partition_value *part, u_int32_t cluster, char *buf);
void destroy_dir_value(struct dir_value *dir_val);
struct dir_value *read_dir(u_int32_t first_cluster, struct partition_value *value);
int32_t change_dir(struct partition_value *value, const u_char *dir_name);
struct partition_value *open_partition(const char *partition);
void close_partition(struct partition_value *part);
#endif //LAB1_FAT32_LIB_H
