#include <stdio.h>
#include <stdint.h>

#define MULTIBOOT2_HEADER_MAGIC         0xe85250d6

const unsigned int multiboot_header[]  __attribute__((section(".multiboot"))) = {MULTIBOOT2_HEADER_MAGIC, 0, 16, -(16+MULTIBOOT2_HEADER_MAGIC), 0, 12};

uint8_t inb (uint16_t _port) {
    uint8_t rv;
    __asm__ __volatile__ ("inb %1, %0" : "=a" (rv) : "dN" (_port));
    return rv;
}

struct termbuf {
    char ASCII;
    char COLOR;
};

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define DEFAULT_COLOR 7 // grey on black 
#define VIDEO_MEM ((struct termbuf*)0xb8000)

int x = 0;
int y = 0;

void print_string(void (*pc)(char), char *s) {
    while (*s != 0) {
        pc(*s);
        s++;
    }
}

void print_char(char c) {
    struct termbuf *vram = (struct termbuf *)0xB8000;
    vram[x].ASCII = c;
    vram[x].COLOR = 7;
    x++;
}

void scroll() {
    // Scroll the screen up by one row
    for (int y = 1; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VIDEO_MEM[(y - 1) * VGA_WIDTH + x] = VIDEO_MEM[y * VGA_WIDTH + x];
        }
    }
    // Clear the last row
    for ( int x = 0; x < VGA_WIDTH; x++) {
        VIDEO_MEM[(VGA_HEIGHT - 1) * VGA_WIDTH + x].ASCII = ' ';
        VIDEO_MEM[(VGA_HEIGHT - 1) * VGA_WIDTH + x].COLOR = DEFAULT_COLOR;
    }
    if (x >= VGA_HEIGHT) {
        x = VGA_HEIGHT - 1;
    }
}


void putc() {
    char c = 'A'; // Example character to print

    if (c == '\n') {
        // Handle newline
        term_col = 0;
        term_row++;
        if (term_row >= VGA_HEIGHT) {
            scroll();
        }
        return;
    }

    VIDEO_MEM[term_row * VGA_WIDTH + term_col].ASCII = c;
    VIDEO_MEM[term_row * VGA_WIDTH + term_col].COLOR = DEFAULT_COLOR;
    term_col++;
    if (term_col >= VGA_WIDTH) {
        term_col = 0;
        term_row++;
        if (term_row >= VGA_HEIGHT) {
            scroll();
        }
    }
}



void main() {

    esp_printf("Hello, kernel World!\n");

    unsigned short *vram = (unsigned short*)0xb8000; // Base address of video mem
    const unsigned char color = 7; // gray text on black background

    while(1) {
        uint8_t status = inb(0x64);

        if(status & 1) {
            uint8_t scancode = inb(0x60);
            
            if (scancode > 128) {
                continue; // Ignore key releases
            }
            
            esp_printf(print_char, "0x%02x\n    %c\n", scancode);
            // keyboard_map[scancode]
        }
    }
}