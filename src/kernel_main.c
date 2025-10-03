#include "rprintf.h"
// #include "esp_printf.h"
#include <stdint.h>

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

int row_x = 0;
int col_y = 0;


// void print_char(char c) {
//     struct termbuf *vram = (struct termbuf *)0xB8000;
//     vram[x].ASCII = c;
//     vram[x].COLOR = 7;
//     x++;
// }

// Pointer to the start of video memory
struct termbuf* const vram = (struct termbuf*)VGA_ADDRESS;

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
        vram[(VGA_HEIGHT - 1) * VGA_WIDTH + x].COLOR = 7;
    }
}


int putc(int ch) {
    // Handle newline character
    if (ch == '\n') {
        row_x++;
        col_y = 0;
    } else {
        vram[row_x * VGA_WIDTH + col_y].ASCII = ch;
        vram[row_x * VGA_WIDTH + col_y].COLOR = 7; 
        row_x++;
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
}


void main() {

    esp_printf(putc, "Hello, World!\n");


    while(1) {
        uint8_t status = inb(0x64);

        if(status & 1) {
            uint8_t scancode = inb(0x60);
            
            if (scancode > 128) {
                continue; // Ignore key releases
            }
            
            esp_printf(putc, "0x%02x\n    %c\n", scancode);
            // keyboard_map[scancode]
        }
    }
}