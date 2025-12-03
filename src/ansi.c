// ansi.c - minimal ANSI escape interpreter for VGA text mode

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
static int inverse_video = 0;    // for \x1b[7m / \x1b[m

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

    // If no explicit params, treat as [0 or [1 depending on command
    int p0 = (nparams >= 1) ? params[0] : 0;
    int p1 = (nparams >= 2) ? params[1] : 0;

    switch (final_ch) {
        case 'J':
            // Erase in display. Kilo uses ESC[2J for clear screen.
            if (p0 == 2 || p0 == 0) {
                clear_screen();
            }
            break;

        case 'K':
            // Erase in line: Kilo uses ESC[K (clear to end of line)
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
            // Select Graphic Rendition. We only handle 0 (reset) and 7 (inverse).
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
            // Ignore unknown CSI sequences
            break;
    }
}

// For CSI ? sequences, Kilo uses ESC[?25l/h to hide/show cursor.
// We won't actually hide the cursor in hardware; we just ignore them.
static void handle_csi_qmark_final(char final_ch) {
    (void)final_ch;
    // could parse params[0] == 25 for show/hide if you later implement it
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
                cur_col = 0;
            } else if (c == '\n') {
                cur_row++;
                if (cur_row >= VGA_HEIGHT) {
                    scroll_up();
                    cur_row = VGA_HEIGHT - 1;
                }
            } else {
                put_char_at_cursor((char)c);
            }
            break;

        case S_ESC:
            if (c == '[') {
                state = S_CSI;
                reset_params();
            } else {
                // Unknown escape, go back to normal
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
                // Final byte
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
                // Final byte (likely 'h' or 'l')
                handle_csi_qmark_final((char)c);
                state = S_NORMAL;
                reset_params();
            }
            break;
    }

    return ch;
}
