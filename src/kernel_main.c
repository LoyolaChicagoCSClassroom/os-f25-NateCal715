
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

static int term_row = 0;
static int term_col = 0;



void putc(int data) {
    char c = (char)(data & 0xFF);

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

void scroll() {
    // Scroll the screen up by one row
    for (int y = 1; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VIDEO_MEM[(y - 1) * VGA_WIDTH + x] = VIDEO_MEM[y * VGA_WIDTH + x];
        }
    }
    // Clear the last row
    for (int x = 0; x < VGA_WIDTH; x++) {
        VIDEO_MEM[(VGA_HEIGHT - 1) * VGA_WIDTH + x].ASCII = ' ';
        VIDEO_MEM[(VGA_HEIGHT - 1) * VGA_WIDTH + x].COLOR = DEFAULT_COLOR;
    }
    if (term_row > 0) term_row--;
}

void main() {
    
    const char *str = "Hello, Kernel World!\n";
    for (const char *p = str; *p != '\0'; p++) {
        putc((int)(*p));
    }
    
    unsigned short *vram = (unsigned short*)0xb8000; // Base address of video mem
    const unsigned char color = 7; // gray text on black background

    while(1) {
        uint8_t status = inb(0x64);

        if(status & 1) {
            uint8_t scancode = inb(0x60);
        }
    }
}
