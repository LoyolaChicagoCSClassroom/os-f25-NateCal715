// ansi.h - tiny ANSI/VT100-style interpreter for VGA text mode

#ifndef ANSI_H
#define ANSI_H

#include <stdint.h>

void ansi_init(void);        // initialize screen and internal state
int  ansi_putc(int ch);      // main entry: feed one character

#endif
