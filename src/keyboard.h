#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

// Arrow key codes
#define ARROW_UP    1000
#define ARROW_LEFT  1001
#define ARROW_RIGHT  1002
#define ARROW_DOWN 1003

int kbd_read_char(void); // Reads a character from the keyboard

extern uint8_t inb(uint16_t _port);

#endif // KEYBOARD_H