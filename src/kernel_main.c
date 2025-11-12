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


// void print_char(char c) {
//     struct termbuf *vram = (struct termbuf *)0xB8000;
//     vram[x].ASCII = c;
//     vram[x].COLOR = 7;
//     x++;
// }

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

void fatInit() {
    // Read boot sector and RDE region
    sector_read(2048, boot_sector);

    printf("Number of bytes per sector = %d\n", ((struct boot_sector*)boot_sector)->bytes_per_sector);
    printf("Number of sectors per cluster = %d\n", ((struct boot_sector*)boot_sector)->num_sectors_per_cluster);
    printf("Number of reserved sectors = %d\n", ((struct boot_sector*)boot_sector)->num_reserved_sectors);
    printf("Number of FAT tables = %d\n", ((struct boot_sector*)boot_sector)->num_fat_tables);
    printf("Number of RDEs = %d\n", ((struct boot_sector*)boot_sector)->num_root_dir_entries);

    root_dir_region_start = 2048
                        + ((struct boot_sector*)boot_sector)->num_reserved_sectors
                        + ((struct boot_sector*)boot_sector)->num_fat_tables * ((struct boot_sector*)boot_sector)->num_sectors_per_fat;

    printf("Root directory region start (sectors) = %d\n", root_dir_region_start);

    sector_read(root_dir_region_start, root_directory_region);
    
}

// fatOpen()
// Find the RDE for a file given a path
struct rde * fatOpen(char *path) {

    struct rde *rde = (struct rde *)root_directory_region;
    //Iterate through the RDE region searching for a file's RDE
    for (int k = 0; k < 10; k++) {
        printf("File name: \"%s.%s\"\n", rde[k].file_name, rde[k].file_extension);
        printf("Data cluster: %d\n", rde[k].cluster);
        printf("File size: %d\n", rde[k].file_size);

        // TODO: Compare with path and return matching entry
    }
    return NULL; // Return NULL if file not found
}

int fatRead(struct rde *rde, char * buf, int n) {
    // read file data into buf from file described by rde
    if (rde == NULL) {
        return -1; // Error: invalid RDE pointer
    }
    // TODO: Implement file reading logic
    // 1. Get starting cluster from rde->cluster
    // 2. Calculate data region start
    // 3. Read sectors corresponding to clusters
    // 4. Follow FAT chain if file spans multiple clusters
    return 0; // Return number of bytes read
}

int main() {

    esp_printf(MyPutC, "Hello, World!\n");


    // Three calls to FAT32 functions
    // fatInit() // Initializes the FAT filesystem driver by reading the superblock (aka boot sector) and FAT into memory.
    // fatOpen() // Opens a file in a FAT filesystem on disk.
    // fatRead() // Reads data from a file into a buffer
    char dataBuf[100];
    struct rde *file_rde;

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
