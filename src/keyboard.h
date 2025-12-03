#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

enum editorKey {
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

int kbd_read_char(void); // Reads a character from the keyboard

extern uint8_t inb(uint16_t _port);

#endif // KEYBOARD_H