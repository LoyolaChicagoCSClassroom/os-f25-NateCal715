#include <stdint.h>
#include "keyboard.h"

extern uint8_t inb(uint16_t _port); 

static const char keyboard_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',    // 0-9
  '9', '0', '-', '=', '\b',    // Backspace
 '\t',          // Tab
 'q', 'w', 'e', 'r',    // 10-19
  't', 'y', 'u', 'i', 'o', 'p',
 '[', ']', '\n',    // Enter key
   0,          // Control key
 'a', 's', 'd', 'f',    // 20-29
  'g', 'h', 'j', 'k', 'l',
 ';', '\'', '`',
   0,          // Left shift
 '\\','z','x','c','v',    // 30-39
  'b','n','m',',','.','/',
   0,          // Right shift
  '*',
   0,    // Alt key
  ' ',    // Space bar
   0,    // Caps lock
   
};

static int waiting_for_extended = 0;

int kbd_read_char(void) {
    for (;;) {
        uint8_t status = inb(0x64);

        if (status & 1) {
            uint8_t scancode = inb(0x60);

            if (scancode == 0xE0) {
                waiting_for_extended = 1;
                continue;
            }

            if (waiting_for_extended) {
                waiting_for_extended = 0;

                if (scancode <= 0x7F) {
                    switch(scancode) {
                        case 0x48: return ARROW_UP;
                        case 0x4B: return ARROW_LEFT;
                        case 0x4D: return ARROW_RIGHT;
                        case 0x50: return ARROW_DOWN;
                    }
                }
                continue;
            }
            
            if (scancode > 128) {
                continue; // Ignore key releases
            }
            
            char ch = keyboard_map[scancode];
            if (ch != 0) {
                return ch;
            }
        }
    }
}