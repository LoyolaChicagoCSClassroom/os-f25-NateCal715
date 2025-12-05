#include <stdint.h>
#include "rprintf.h"
#include "fat.h"
#include "ide.h"
#include "ansi.h"
#include "KILO.h"
#include "keyboard.h"

#define MULTIBOOT2_HEADER_MAGIC 0xe85250d6
#define SECTOR_SIZE 512

const unsigned int multiboot_header[] __attribute__((section(".multiboot"))) = {
    MULTIBOOT2_HEADER_MAGIC, 0, 16, -(16 + MULTIBOOT2_HEADER_MAGIC), 0, 12
};

uint8_t inb(uint16_t _port) {
    uint8_t rv;
    __asm__ __volatile__ ("inb %1, %0" : "=a" (rv) : "dN"(_port));
    return rv;
}

int MyPutC(int ch) {   
    return ansi_putc(ch);
}

int root_dir_region_start = 0;
int data_region_start = 0;
char boot_sector[512];
char root_directory_region[512];
char fat_table[512 * 32];  // Cache for FAT table

extern void *memcpy(void *dest, const void *src, size_t n);

void sector_read(unsigned int lba, void *buffer) {
    if (ata_lba_read(lba, (unsigned char *)buffer, 1) != 0) {
        esp_printf(MyPutC, "Error reading sector %d\n", lba);
    }
}

void sector_write(unsigned int lba, void *buffer) {
    // Note: You'll need to implement ata_lba_write in ide.c
    // For now, this is a placeholder
    (void)lba;
    (void)buffer;
}

void fatInit() {
    sector_read(2048, boot_sector);

    struct boot_sector *bs = (struct boot_sector*)boot_sector;

    root_dir_region_start = 2048
                        + bs->num_reserved_sectors
                        + bs->num_fat_tables * bs->num_sectors_per_fat;

    int root_dir_sectors = (bs->num_root_dir_entries * 32 + bs->bytes_per_sector - 1) / bs->bytes_per_sector;
    data_region_start = 2048 + bs->num_reserved_sectors 
                        + (bs->num_fat_tables * bs->num_sectors_per_fat) 
                        + root_dir_sectors;

    sector_read(root_dir_region_start, root_directory_region);
    
    // Load FAT table into cache
    int fat_start = 2048 + bs->num_reserved_sectors;
    for (int i = 0; i < bs->num_sectors_per_fat && i < 32; i++) {
        sector_read(fat_start + i, fat_table + (i * 512));
    }
}

rde * fatOpen(char *path) {
    rde *rde_entries = (rde *)root_directory_region;

    char target_name[8]; 
    for (int i = 0; i < 8; i++) target_name[i] = ' ';
    char target_ext[3]; 
    for (int i = 0; i < 3; i++) target_ext[i] = ' ';

    int dot_pos = -1;
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '.') {
            dot_pos = i;
            break;
        }
    }

    int name_len = (dot_pos >= 0) ? dot_pos : 0;
    if (dot_pos < 0) {
        while (path[name_len] != '\0') name_len++;
    }

    if (name_len > 8) name_len = 8;

    for (int i = 0; i < name_len; i++) {
        target_name[i] = (path[i] >= 'a' && path[i] <= 'z') ? (path[i] - 32) : path[i];
    }

    if (dot_pos >= 0) {
        int ext_start = dot_pos + 1;
        for (int i = 0; i < 3 && path[ext_start + i] != '\0'; i++) {
            target_ext[i] = (path[ext_start + i] >= 'a' && path[ext_start + i] <= 'z') 
            ? (path[ext_start + i] - 32) 
            : path[ext_start + i];
        }
    }

    int max_entries = ((struct boot_sector*)boot_sector)->num_root_dir_entries;

    for (int k = 0; k < max_entries; k++) {
        if (rde_entries[k].file_name[0] == 0x00) break;
        if ((unsigned char)rde_entries[k].file_name[0] == 0xE5) continue;

        int name_match = 1;
        for (int i = 0; i < 8; i++) {
            if (rde_entries[k].file_name[i] != target_name[i]) {
                name_match = 0;
                break;
            }
        }
    
        int ext_match = 1;
        for (int i = 0; i < 3; i++) {
            if (rde_entries[k].file_extension[i] != target_ext[i]) {
                ext_match = 0;
                break;
            }
        }

        if (name_match && ext_match) {
            return &rde_entries[k];
        }
    }
    return (rde*)0;
}

int fatRead(rde *rde_ptr, char * buf, int n) {
    if (rde_ptr == (rde*)0) {
        return -1;
    }
    
    struct boot_sector *bs = (struct boot_sector*)boot_sector;
    
    uint16_t cluster = rde_ptr->cluster;
    int first_sector = data_region_start + (cluster - 2) * bs->num_sectors_per_cluster;
    
    char cluster_buf[CLUSTER_SIZE];
    for (int i = 0; i < bs->num_sectors_per_cluster; i++) {
        sector_read(first_sector + i, cluster_buf + (i * SECTOR_SIZE));
    }
    
    int bytes_to_copy = (n < (int)rde_ptr->file_size) ? n : (int)rde_ptr->file_size;
    if (bytes_to_copy > CLUSTER_SIZE) bytes_to_copy = CLUSTER_SIZE;
    
    for (int i = 0; i < bytes_to_copy; i++) {
        buf[i] = cluster_buf[i];
    }
    
    if (bytes_to_copy < n) {
        buf[bytes_to_copy] = '\0';
    }

    return bytes_to_copy;
}

// Find a free cluster in the FAT
uint16_t find_free_cluster() {
    struct boot_sector *bs = (struct boot_sector*)boot_sector;
    uint16_t *fat = (uint16_t*)fat_table;
    
    // Start from cluster 2 (first usable cluster)
    int max_clusters = (bs->num_sectors_per_fat * 512) / 2;
    for (int i = 2; i < max_clusters; i++) {
        if (fat[i] == 0) {
            return i;
        }
    }
    return 0; // No free cluster found
}

// Find or create an RDE for a file
rde* find_or_create_rde(char *filename) {
    rde *rde_entries = (rde *)root_directory_region;
    struct boot_sector *bs = (struct boot_sector*)boot_sector;
    int max_entries = bs->num_root_dir_entries;
    
    // Parse filename into name and extension
    char target_name[8]; 
    for (int i = 0; i < 8; i++) target_name[i] = ' ';
    char target_ext[3]; 
    for (int i = 0; i < 3; i++) target_ext[i] = ' ';

    int dot_pos = -1;
    for (int i = 0; filename[i] != '\0'; i++) {
        if (filename[i] == '.') {
            dot_pos = i;
            break;
        }
    }

    int name_len = (dot_pos >= 0) ? dot_pos : 0;
    if (dot_pos < 0) {
        while (filename[name_len] != '\0') name_len++;
    }
    if (name_len > 8) name_len = 8;

    for (int i = 0; i < name_len; i++) {
        target_name[i] = (filename[i] >= 'a' && filename[i] <= 'z') ? (filename[i] - 32) : filename[i];
    }

    if (dot_pos >= 0) {
        int ext_start = dot_pos + 1;
        for (int i = 0; i < 3 && filename[ext_start + i] != '\0'; i++) {
            target_ext[i] = (filename[ext_start + i] >= 'a' && filename[ext_start + i] <= 'z') 
                ? (filename[ext_start + i] - 32) 
                : filename[ext_start + i];
        }
    }
    
    // First, try to find existing entry
    for (int k = 0; k < max_entries; k++) {
        if (rde_entries[k].file_name[0] == 0x00 || 
            (unsigned char)rde_entries[k].file_name[0] == 0xE5) {
            continue;
        }
        
        int name_match = 1;
        for (int i = 0; i < 8; i++) {
            if (rde_entries[k].file_name[i] != target_name[i]) {
                name_match = 0;
                break;
            }
        }
        
        int ext_match = 1;
        for (int i = 0; i < 3; i++) {
            if (rde_entries[k].file_extension[i] != target_ext[i]) {
                ext_match = 0;
                break;
            }
        }
        
        if (name_match && ext_match) {
            return &rde_entries[k];
        }
    }
    
    // File doesn't exist, create new entry
    for (int k = 0; k < max_entries; k++) {
        if (rde_entries[k].file_name[0] == 0x00 || 
            (unsigned char)rde_entries[k].file_name[0] == 0xE5) {
            // Found free slot
            for (int i = 0; i < 8; i++) rde_entries[k].file_name[i] = target_name[i];
            for (int i = 0; i < 3; i++) rde_entries[k].file_extension[i] = target_ext[i];
            rde_entries[k].attribute = 0;
            rde_entries[k].reserved1 = 0;
            rde_entries[k].creation_timestamp = 0;
            rde_entries[k].creation_time = 0;
            rde_entries[k].creation_date = 0;
            rde_entries[k].access_date = 0;
            rde_entries[k].reserved2 = 0;
            rde_entries[k].modified_time = 0;
            rde_entries[k].modified_date = 0;
            rde_entries[k].cluster = 0;
            rde_entries[k].file_size = 0;
            
            return &rde_entries[k];
        }
    }
    
    return (rde*)0; // No free RDE slots
}

int fatWrite(char *filename, char *data, int size) {
    // Find or create RDE for this file
    rde *entry = find_or_create_rde(filename);
    if (!entry) {
        return -1; // No free directory entries
    }
    
    struct boot_sector *bs = (struct boot_sector*)boot_sector;
    
    // If file needs a new cluster or doesn't have one
    if (entry->cluster == 0 || size > CLUSTER_SIZE) {
        uint16_t cluster = find_free_cluster();
        if (cluster == 0) {
            return -2; // No free clusters
        }
        
        // Mark cluster as end-of-chain in FAT
        uint16_t *fat = (uint16_t*)fat_table;
        fat[cluster] = 0xFFFF;
        
        entry->cluster = cluster;
    }
    
    // Update file size
    entry->file_size = size;
    
    // Write data to cluster
    int first_sector = data_region_start + (entry->cluster - 2) * bs->num_sectors_per_cluster;
    
    char cluster_buf[CLUSTER_SIZE];
    // Clear buffer
    for (int i = 0; i < CLUSTER_SIZE; i++) cluster_buf[i] = 0;
    
    // Copy data to buffer
    int bytes_to_write = (size < CLUSTER_SIZE) ? size : CLUSTER_SIZE;
    for (int i = 0; i < bytes_to_write; i++) {
        cluster_buf[i] = data[i];
    }
    
    // Write sectors
    // NOTE: This requires implementing sector_write which needs ata_lba_write
    // For now, we'll just show the structure
    for (int i = 0; i < bs->num_sectors_per_cluster; i++) {
        sector_write(first_sector + i, cluster_buf + (i * SECTOR_SIZE));
    }
    
    // Write updated root directory back to disk
    sector_write(root_dir_region_start, root_directory_region);
    
    // Write updated FAT back to disk
    int fat_start = 2048 + bs->num_reserved_sectors;
    for (int i = 0; i < bs->num_sectors_per_fat && i < 32; i++) {
        sector_write(fat_start + i, fat_table + (i * 512));
    }
    
    return bytes_to_write;
}

int main() {
    ansi_init();

    esp_printf(MyPutC, "Booting kernel...\n");
    
    fatInit();

    rde *file = fatOpen("file.txt");
    if (file) {
        char dataBuf[100];
        int n = fatRead(file, dataBuf, sizeof(dataBuf) - 1);
        if (n > 0) {
            dataBuf[n] = '\0';
        } else {
            esp_printf(MyPutC, "Error reading file.\n");
        }
    } else {
        esp_printf(MyPutC, "File not found (will be created on save).\n");
    }
    
    esp_printf(MyPutC, "Press 1 for KILO editor, 2 for keyboard debug: ");
    int choice = kbd_read_char();
    MyPutC('\n');

    ansi_init();

    if (choice == '1') {
        esp_printf(MyPutC, "Starting KILO editor...\n");
        kilo_run("file.txt");
        esp_printf(MyPutC, "KILO exited.\n");
    } else {
        esp_printf(MyPutC, "Keyboard debug mode. Press keys (Ctrl+Q to exit).\n");
        while (1) {
            int c = kbd_read_char();
            
            if (c == '\r' || c == '\n') {
                MyPutC('\r');
                MyPutC('\n');
            } else if (c == '\b' || c == 127) {
                esp_printf(MyPutC, "[BS]");
                MyPutC('\b');
            } else if (c >= 32 && c < 127) {
                MyPutC(c);
            } else if (c >= 1000) {
                switch(c) {
                    case 1000: esp_printf(MyPutC, "[LEFT]"); break;
                    case 1001: esp_printf(MyPutC, "[RIGHT]"); break;
                    case 1002: esp_printf(MyPutC, "[UP]"); break;
                    case 1003: esp_printf(MyPutC, "[DOWN]"); break;
                    case 1004: esp_printf(MyPutC, "[DEL]"); break;
                    case 1005: esp_printf(MyPutC, "[HOME]"); break;
                    case 1006: esp_printf(MyPutC, "[END]"); break;
                    case 1007: esp_printf(MyPutC, "[PGUP]"); break;
                    case 1008: esp_printf(MyPutC, "[PGDN]"); break;
                    default: esp_printf(MyPutC, "[%d]", c); break;
                }
            } else if (c == 17) {
                esp_printf(MyPutC, "[CTRL+Q]");
                break;
            } else {
                esp_printf(MyPutC, "[0x%x]", c);
            }
        }
    }
    
    return 0;
}