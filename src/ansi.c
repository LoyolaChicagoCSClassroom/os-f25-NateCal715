// ansi.c - minimal ANSI escape interpreter for VGA text mode, KILO-compatible

#include "ansi.h"

#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_ADDRESS 0xb8000

struct termbuf {
    char ASCII;
    char COLOR;
};

// Video memory
static struct termbuf *const vram = (struct termbuf*)VGA_ADDRESS;

// Cursor & state
static int cur_row = 0;
static int cur_col = 0;
static int inverse_video = 0;    // for ESC[7m / ESC[0m

// --- low-level helpers ------------------------------------------------

static void scroll_up(void) {
    // scroll screen up by one row
    for (int r = 1; r < VGA_HEIGHT; ++r) {
        for (int c = 0; c < VGA_WIDTH; ++c) {
            vram[(r - 1) * VGA_WIDTH + c] = vram[r * VGA_WIDTH + c];
        }
    }
    // clear last row
    for (int c = 0; c < VGA_WIDTH; ++c) {
        vram[(VGA_HEIGHT - 1) * VGA_WIDTH + c].ASCII = ' ';
        vram[(VGA_HEIGHT - 1) * VGA_WIDTH + c].COLOR = 7;
    }
}

static void set_cursor(int row, int col) {
    if (row < 0) row = 0;
    if (row >= VGA_HEIGHT) row = VGA_HEIGHT - 1;
    if (col < 0) col = 0;
    if (col >= VGA_WIDTH) col = VGA_WIDTH - 1;
    cur_row = row;
    cur_col = col;
}

static void clear_screen(void) {
    for (int r = 0; r < VGA_HEIGHT; ++r) {
        for (int c = 0; c < VGA_WIDTH; ++c) {
            vram[r * VGA_WIDTH + c].ASCII = ' ';
            vram[r * VGA_WIDTH + c].COLOR = 7;
        }
    }
    set_cursor(0, 0);
}

static void clear_to_eol(void) {
    for (int c = cur_col; c < VGA_WIDTH; ++c) {
        vram[cur_row * VGA_WIDTH + c].ASCII = ' ';
        vram[cur_row * VGA_WIDTH + c].COLOR = 7;
    }
}

static void handle_backspace(void) {
    // Move cursor back and erase that cell
    if (cur_col > 0) {
        cur_col--;
    } else if (cur_row > 0) {
        cur_row--;
        cur_col = VGA_WIDTH - 1;
    }
    vram[cur_row * VGA_WIDTH + cur_col].ASCII = ' ';
    vram[cur_row * VGA_WIDTH + cur_col].COLOR = 7;
}

static void put_char_at_cursor(char ch) {
    vram[cur_row * VGA_WIDTH + cur_col].ASCII = ch;
    vram[cur_row * VGA_WIDTH + cur_col].COLOR = inverse_video ? 0x70 : 0x07;

    cur_col++;
    if (cur_col >= VGA_WIDTH) {
        cur_col = 0;
        cur_row++;
        if (cur_row >= VGA_HEIGHT) {
            scroll_up();
            cur_row = VGA_HEIGHT - 1;
        }
    }
}

// --- ANSI state machine -----------------------------------------------

typedef enum {
    S_NORMAL = 0,
    S_ESC,
    S_CSI,
    S_CSI_QMARK
} ansi_state_t;

static ansi_state_t state = S_NORMAL;
static int params[4];
static int nparams;
static int cur_param;    // -1 => none so far

static void reset_params(void) {
    for (int i = 0; i < 4; ++i) params[i] = 0;
    nparams = 0;
    cur_param = -1;
}

static void finish_param(void) {
    if (cur_param < 0) return;
    if (nparams < 4) {
        params[nparams++] = cur_param;
    }
    cur_param = -1;
}

static void handle_csi_final(char final_ch) {
    finish_param();

    int p0 = (nparams >= 1) ? params[0] : 0;
    int p1 = (nparams >= 2) ? params[1] : 0;

    switch (final_ch) {
        case 'J':
            // Erase in display. KILO uses ESC[2J for clear screen.
            if (p0 == 2 || p0 == 0) {
                clear_screen();
            }
            break;

        case 'K':
            // Erase in line: ESC[K (clear to end of line)
            clear_to_eol();
            break;

        case 'H':
        case 'f': {
            // Cursor position: ESC[row;colH
            int row = (p0 > 0 ? p0 : 1) - 1;
            int col = (p1 > 0 ? p1 : 1) - 1;
            set_cursor(row, col);
            break;
        }

        case 'm':
            // Select Graphic Rendition. Handle 0 (reset) and 7 (inverse).
            if (nparams == 0) {
                inverse_video = 0;
            } else {
                for (int i = 0; i < nparams; ++i) {
                    if (params[i] == 0) {
                        inverse_video = 0;
                    } else if (params[i] == 7) {
                        inverse_video = 1;
                    }
                }
            }
            break;

        default:
            // Ignore other CSI sequences
            break;
    }
}

// For CSI ? sequences, KILO uses ESC[?25l/h to hide/show cursor.
// We ignore those for now (no hardware cursor control here).
static void handle_csi_qmark_final(char final_ch) {
    (void)final_ch;
}

// --- public API -------------------------------------------------------

void ansi_init(void) {
    state = S_NORMAL;
    inverse_video = 0;
    reset_params();
    clear_screen();
}

int ansi_putc(int ch) {
    unsigned char c = (unsigned char) ch;

    switch (state) {
        case S_NORMAL:
            if (c == 0x1B) {
                // ESC
                state = S_ESC;
            } else if (c == '\r') {
                // Carriage return: start of line
                cur_col = 0;
            } else if (c == '\n') {
                // Newline: move down; assume CR was already sent
                cur_row++;
                if (cur_row >= VGA_HEIGHT) {
                    scroll_up();
                    cur_row = VGA_HEIGHT - 1;
                }
            } else if (c == '\b' || c == 0x7F) {
                handle_backspace();
            } else {
                // Printable or other bytes
                put_char_at_cursor((char)c);
            }
            break;

        case S_ESC:
            if (c == '[') {
                state = S_CSI;
                reset_params();
            } else {
                // Unknown single-char escape; ignore
                state = S_NORMAL;
            }
            break;

        case S_CSI:
            if (c == '?') {
                state = S_CSI_QMARK;
                reset_params();
            } else if (c >= '0' && c <= '9') {
                int digit = c - '0';
                if (cur_param < 0) cur_param = 0;
                cur_param = cur_param * 10 + digit;
            } else if (c == ';') {
                finish_param();
            } else {
                // Final CSI byte
                handle_csi_final((char)c);
                state = S_NORMAL;
                reset_params();
            }
            break;

        case S_CSI_QMARK:
            if (c >= '0' && c <= '9') {
                int digit = c - '0';
                if (cur_param < 0) cur_param = 0;
                cur_param = cur_param * 10 + digit;
            } else if (c == ';') {
                finish_param();
            } else {
                // Final byte for ESC[? sequences (?25h / ?25l)
                handle_csi_qmark_final((char)c);
                state = S_NORMAL;
                reset_params();
            }
            break;
    }

    return ch;
}
