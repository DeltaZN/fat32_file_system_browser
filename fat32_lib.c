//
// Created by gosha on 12.02.2021.
//

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct fat_BS {
    unsigned char bootjmp[3];
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

    unsigned int		table_size_32;
    unsigned short		extended_flags;
    unsigned short		fat_version;
    unsigned int		root_cluster;
    unsigned short		fat_info;
    unsigned short		backup_BS_sector;
    unsigned char 		reserved_0[12];
    unsigned char		drive_number;
    unsigned char 		reserved_1;
    unsigned char		boot_signature;
    unsigned int 		volume_id;
    unsigned char		volume_label[11];
    unsigned char		fat_type_label[8];

}__attribute__((packed)) fat_BS_t;

typedef struct fs_info {
    unsigned int lead_signature;
    unsigned char reserved_0[480];
    unsigned int another_signature;
    unsigned int last_free_cluster;
    unsigned int available;
    unsigned char reserved_1[12];
    unsigned int tail_signature;
}__attribute__((packed)) fs_info_t;

typedef struct dir_entry {
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
}__attribute__((packed)) dir_entry_t;

typedef struct long_fname {
    unsigned char order;
    unsigned char _2_byte_chars[10];
    unsigned char attribute;
    unsigned char long_entry_type;
    unsigned char checksum;
    unsigned char _2_byte_chars_next[12];
    unsigned short always_zero;
    unsigned char _2_byte_chars_last[4];
}__attribute__((packed)) long_fname_t;

unsigned int get_fat_table_value(unsigned int active_cluster, unsigned int first_fat_sector, unsigned int sector_size, int fd) {
    unsigned char *FAT_table = malloc(sector_size);
    unsigned int fat_offset = active_cluster * 4;
    unsigned int fat_sector = first_fat_sector + (fat_offset / sector_size);
    unsigned int ent_offset = fat_offset % sector_size;
    pread(fd, FAT_table, sector_size, fat_sector * sector_size);
    unsigned int table_value = *(unsigned int*)&FAT_table[ent_offset] & 0x0FFFFFFF;
    free(FAT_table);
    return table_value;
}

typedef struct dir_value {
    unsigned char *filename;
    unsigned char type;
    unsigned int size;
    void *next;
}__attribute__((packed)) dir_value_t;

void read_dir(unsigned int first_sector, unsigned int cluster_size, int sector_size, int fd) {
    dir_entry_t *buf = malloc(cluster_size);
    pread(fd, buf, cluster_size, first_sector * sector_size);
    dir_value_t *first_dir_value;
    dir_value_t *current_dir_value;

    char *order_bitmap = malloc(32);
    char *order[32];
    unsigned int long_name_counter = 0;
    char end_dir_reached = 0;
    int j = 0;
    while (!end_dir_reached) {
        dir_entry_t *entry = &buf[j++];
        if (entry->file_name[0] == 0) {
            // dir end
            end_dir_reached = 1;
        } else if (entry->file_name[0] == 0xE5) {
            // unused entry - skip
            continue;
        } else if (entry->attributes == 0x0F) {
            long_fname_t *fname = (long_fname_t *) entry;
            // maximum order value == 0x1F
            int current_order = fname->order & 0x001F;
            order[current_order] = malloc(13);
            order_bitmap[current_order] = 1;
            char *current_buf = order[current_order];
            int buf_offset = 0;
            for (int i = 0; i < 10; i += 2) {
                current_buf[buf_offset++] = fname->_2_byte_chars[i];
            }
            for (int i = 0; i < 12; i += 2) {
                current_buf[buf_offset++] = fname->_2_byte_chars_next[i];
            }
            for (int i = 0; i < 4; i += 2) {
                current_buf[buf_offset++] = fname->_2_byte_chars_last[i];
            }
            long_name_counter++;
        } else if (entry->attributes == 0x10 || entry->attributes == 0x20 || entry->attributes == 0x16) {
            if (!long_name_counter) {
                char tmp_name[9];
                char tmp_ext[4];
                strncpy(tmp_name, entry->file_name, 8);
                strncpy(tmp_ext, entry->extension, 3);
                for (int i = 7; i >= 0; --i) {
                    if (tmp_name[i] == 32) {
                        tmp_name[i] = 0;
                    } else {
                        break;
                    }
                }
                if (entry->attributes == 0x20) {
                    printf("%s.%s %d b\n", tmp_name, tmp_ext, entry->file_size);
                } else {
                    printf("%s\n", tmp_name);
                }
            } else {
                char *tmp_str = malloc(512);
                for (int i = 0; i < 32; ++i) {
                    if (order_bitmap[i] == 1) {
                        strcat(tmp_str, order[i]);
                        order_bitmap[i] = 0;
                        free(order[i]);
                    }
                }
                long_name_counter = 0;
                if (entry->attributes == 20) {
                    printf("%s %d b\n", tmp_str, entry->file_size);
                } else {
                    printf("%s\n", tmp_str);
                }
                free(tmp_str);
            }
        }
    }

    free(order_bitmap);
    free(buf);
}


void test_read_sda1() {
    FILE *f = fopen("/dev/sdc1", "rb");
    int fd = open("/dev/sdc1", O_RDONLY, 00666);
    struct stat fstat;
    stat("/dev/sdc1", &fstat);
    int blksize = (int) fstat.st_blksize;
    fat_BS_t *fat_boot;
    long length;

    if (f) {
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        fseek(f, 0, SEEK_SET);
        // to be sure we have zero byte at the end
        fat_boot = malloc(sizeof(fat_BS_t));
        pread(fd, fat_boot, sizeof(fat_BS_t), 0);
        unsigned int total_sectors = fat_boot->total_sectors_32;
        unsigned int fat_size = (fat_boot->table_size_16 == 0) ? fat_boot->table_size_32 : fat_boot->table_size_16;
        unsigned int root_dir_sectors =
                ((fat_boot->root_entry_count * 32) + (fat_boot->bytes_per_sector - 1)) / fat_boot->bytes_per_sector;
        unsigned int first_data_sector = fat_boot->reserved_sector_count + (fat_boot->table_count * fat_size) + root_dir_sectors;
        unsigned int first_fat_sector = fat_boot->reserved_sector_count;
        unsigned int data_sectors = total_sectors -
                           (fat_boot->reserved_sector_count + (fat_boot->table_count * fat_size) + root_dir_sectors);
        unsigned int total_clusters = data_sectors / fat_boot->sectors_per_cluster;
        if (total_clusters < 4085) {
            printf("FAT12");
        } else if (total_clusters < 65525) {
            printf("FAT16");
        } else if (total_clusters < 268435445) {
            printf("FAT32\n");
        } else {
            printf("ExFAT");
        }
        unsigned int root_cluster_32 = fat_boot->root_cluster;
        unsigned int first_sector_of_root_cluster = ((root_cluster_32 - 2) * fat_boot->sectors_per_cluster) + first_data_sector;
        unsigned int cluster_size = fat_boot->bytes_per_sector * fat_boot->sectors_per_cluster;
//        dir_entry_t *buf = malloc(cluster_size);
//        pread(fd, buf, cluster_size, first_sector_of_root_cluster * fat_boot->bytes_per_sector);
        fs_info_t *fs = malloc(sizeof(fs_info_t));
        pread(fd, fs, sizeof(fs_info_t), fat_boot->fat_info * fat_boot->bytes_per_sector);
//        unsigned int i = get_fat_table_value(root_cluster_32, first_fat_sector, fat_boot->bytes_per_sector, fd);

        read_dir(first_sector_of_root_cluster, cluster_size, fat_boot->bytes_per_sector, fd);
        free(fs);
        free(fat_boot);

        fclose(f);
    }
}