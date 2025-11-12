#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "rprintf.h"
#include "fat.h"
#include "ide.h"

int fd = 0;
int root_dir_region_start = 0;
char boot_sector[512];
char root_directory_region[512];

#define MULTIBOOT2_HEADER_MAGIC 0xe85250d6

// Multiboot2 header for GRUB
const unsigned int multiboot_header[] __attribute__((section(".multiboot"))) = {
    MULTIBOOT2_HEADER_MAGIC, 0, 16, -(16 + MULTIBOOT2_HEADER_MAGIC), 0, 12
};

uint8_t inb(uint16_t _port) {
    uint8_t rv;
    __asm__ __volatile__ ("inb %1, %0" : "=a" (rv) : "dN"(_port));
    return rv;
}



#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_ADDRESS 0xb8000

struct termbuf {
    char ASCII;
    char COLOR;
};

static int row_x = 0;
static int col_y = 0;


// Pointer to the start of video memory
static struct termbuf* const vram = (struct termbuf*)VGA_ADDRESS;

void scroll() {
    // Scroll the screen up by one row
    for (int x = 1; x < VGA_HEIGHT; x++) {
        for (int y = 0; y < VGA_WIDTH; y++) {
            vram[(x - 1) * VGA_WIDTH + y] = vram[x * VGA_WIDTH + y];
        }
    }
    // Clear the last row
    for (int y = 0; y < VGA_WIDTH; y++) {
        vram[(VGA_HEIGHT - 1) * VGA_WIDTH + y].ASCII = ' ';
        vram[(VGA_HEIGHT - 1) * VGA_WIDTH + y].COLOR = 7;
    }
}


int MyPutC(int ch) {   
    // Handle newline character
    if (ch == '\n') {
        row_x++;
        col_y = 0;
    } else {
        vram[row_x * VGA_WIDTH + col_y].ASCII = (char)ch;
        vram[row_x * VGA_WIDTH + col_y].COLOR = 7; 
        col_y++;
    }

    // Move to the next column
    if (col_y >= VGA_WIDTH) {
        col_y = 0;
        row_x++;
    }
    // Scroll if we reach the bottom of the screen
    if (row_x >= VGA_HEIGHT) {
        scroll();
        row_x = VGA_HEIGHT - 1;
    }

    return ch;
}

    int print_string(void (*pc)(char), char *s) {
        // Print each character until null terminator
        while (*s != 0) {
            uint8_t status = inb(0x64);
            pc(*s);
            s++;
        }
        return 0;
    }

void driver_init(const char *disk_image_path) {
    fd = open(disk_image_path, O_RDONLY);
    if (fd < 0) {
        esp_printf(MyPutC, "Error opening disk image: %s\n", disk_image_path);
    }
}

void sector_read(unsigned int lba, void *buffer) {
    // Read a sector from the disk image into the buffer
    if (lseek(fd, lba * 512, SEEK_SET) < 0) {
        esp_printf(MyPutC, "Error seeking to sector %d\n", lba);
        return;
    }
    if (read(fd, buffer, 512) != 512) {
        esp_printf(MyPutC, "Error reading sector %d\n", lba);
    }
}

void fatInit() {
    // Read boot sector and RDE region
    sector_read(2048, boot_sector);

    esp_printf(MyPutC, "Number of sectors per cluster = %d\n", ((struct boot_sector*)boot_sector)->num_sectors_per_cluster);
    esp_printf(MyPutC, "Number of bytes per sector = %d\n", ((struct boot_sector*)boot_sector)->bytes_per_sector);
    esp_printf(MyPutC, "Number of reserved sectors = %d\n", ((struct boot_sector*)boot_sector)->num_reserved_sectors);
    esp_printf(MyPutC, "Number of FAT tables = %d\n", ((struct boot_sector*)boot_sector)->num_fat_tables);
    esp_printf(MyPutC, "Number of RDEs = %d\n", ((struct boot_sector*)boot_sector)->num_root_dir_entries);

    root_dir_region_start = 2048
                        + ((struct boot_sector*)boot_sector)->num_reserved_sectors
                        + ((struct boot_sector*)boot_sector)->num_fat_tables * ((struct boot_sector*)boot_sector)->num_sectors_per_fat;

    esp_printf(MyPutC, "Root directory region start (sectors) = %d\n", root_dir_region_start);

    sector_read(root_dir_region_start, root_directory_region);
    
}

// fatOpen()
// Find the RDE for a file given a path
rde * fatOpen(char *path) {

    rde *rde_entries = (rde *)root_directory_region;

    char target_name[8];
    char target_ext[3];

    for (int i = 0 ; i < 8; i++) target_name[i] = ' ';
    for (int i = 0 ; i < 3; i++) target_ext[i] = ' ';

    int dot_pos = -1;
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '.') {
            dot_pos = i;
            break;
        }
    }

    int name_len = (dot_pos >= 0) ?  dot_pos : 0;
    if (dot_pos < 0) {
        name_len = 0;
        while (path[name_len] != '\0') name_len++;
    }
    if (name_len > 8) name_len = 8;

    for (int i = 0; i < name_len; i++) {
        target_name[i] = (path[i] >= 'a' && path[i] <= 'z') ? path[i] - 32 : path[i];
    }

    if (dot_pos >= 0) {
        int ext_start = dot_pos + 1;
        for (int i = 0; i < 3 && path[ext_start + i] != '\0'; i++) {
            target_ext[i] = (path[ext_start + i] >= 'a' && path[ext_start + i] <= 'z') 
            ? path[ext_start + i] - 32 
            : path[ext_start + i];
        }
    }

    int max_entries = ((struct boot_sector*)boot_sector)->num_root_dir_entries;

    //Iterate through the RDE region searching for a file's RDE
    for (int k = 0; k < max_entries; k++) {

        if (rde_entries[k].file_name[0] == 0x00) break;

        if ((unsigned char)rde_entries[k].file_name[0] == 0xE5) continue;

        esp_printf(MyPutC, "File name: \"%s.%s\"\n", rde_entries[k].file_name, rde_entries[k].file_extension);
        esp_printf(MyPutC, "Data cluster: %d\n", rde_entries[k].cluster);
        esp_printf(MyPutC, "File size: %d\n", rde_entries[k].file_size);

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
            esp_printf(MyPutC, "Found matching file!\n");
            return &rde_entries[k];
        }
    }
    return (rde*)0; // Return NULL if file not found
}

int fatRead(rde *rde_ptr, char * buf, int n) {
    // read file data into buf from file described by rde
    if (rde_ptr == (rde*)0) {
        return -1; // Error: invalid RDE pointer
    }
    // Get the starting cluster
    uint16_t cluster = rde_ptr->cluster;
    
    // Calculate data region start
    // Data region starts after: boot sector + FAT tables + root directory
    struct boot_sector *bs = (struct boot_sector*)boot_sector;
    int root_dir_sectors = (bs->num_root_dir_entries * 32 + bs->bytes_per_sector - 1) / bs->bytes_per_sector;
    int data_region_start = 2048 + bs->num_reserved_sectors + 
                           (bs->num_fat_tables * bs->num_sectors_per_fat) + 
                           root_dir_sectors;
    
    // Calculate the sector for this cluster
    // Cluster 2 is the first data cluster
    int first_sector = data_region_start + (cluster - 2) * bs->num_sectors_per_cluster;
    
    // Read the data (simplified: only reading first cluster)
    char cluster_buf[CLUSTER_SIZE];
    for (int i = 0; i < bs->num_sectors_per_cluster; i++) {
        sector_read(first_sector + i, cluster_buf + (i * 512));
    }
    
    // Copy to output buffer
    int bytes_to_copy = (n < rde_ptr->file_size) ? n : rde_ptr->file_size;
    if (bytes_to_copy > CLUSTER_SIZE) bytes_to_copy = CLUSTER_SIZE;
    
    for (int i = 0; i < bytes_to_copy; i++) {
        buf[i] = cluster_buf[i];
    }
    buf[bytes_to_copy] = '\0'; // Null terminate if it's a text file
    
    return bytes_to_copy; // Return number of bytes read
}

int main() {

    esp_printf(MyPutC, "Hello, World!\n");


    // Three calls to FAT32 functions
    // fatInit() // Initializes the FAT filesystem driver by reading the superblock (aka boot sector) and FAT into memory.
    // fatOpen() // Opens a file in a FAT filesystem on disk.
    // fatRead() // Reads data from a file into a buffer
    char dataBuf[100];
    rde *file_rde;

    driver_init("disk.img"); // Initializes the IDE driver to read/write sectors from/to the disk.
    fatInit(); // Initializes the FAT filesystem driver by reading the superblock (aka boot sector) and FAT into memory.
    file_rde = fatOpen("file.txt"); // Opens a file in a FAT filesystem on disk
    if (file_rde != NULL) {
        fatRead(file_rde, dataBuf, sizeof(dataBuf)); // Reads data from a file into a buffer
        printf("data read from file = %s\n", dataBuf);
    } else {
        printf("File not found.\n");
    }

    while(1) {
        uint8_t status = inb(0x64);

        if(status & 1) {
            uint8_t scancode = inb(0x60);
            
            if (scancode > 128) {
                continue; // Ignore key releases
            }
            
            esp_printf(MyPutC, "0x%02x\n    %c\n", scancode);
            // keyboard_map[scancode]
        }
    }
    
    return 0;
    
}
