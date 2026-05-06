#include "terminal.h"

/* ── Hardware constants ───────────────────────────────────────────────── */
#define VGA_WIDTH   80u
#define VGA_HEIGHT  25u
#define VGA_MEMORY  ((uint16_t*)0xB8000)

/* ── Driver state ────────────────────────────────────────────────────── */
static uint16_t* terminal_buffer;
static uint16_t  terminal_row;
static uint16_t  terminal_col;
static uint8_t   terminal_color;   /* current attribute byte */

/* ── Internal helpers ────────────────────────────────────────────────── */

/* Write a single cell at absolute position (row, col). */
static inline void put_entry(uint16_t row, uint16_t col, char c, uint8_t color)
{
    terminal_buffer[row * VGA_WIDTH + col] = vga_entry(c, color);
}

/*
 * Scroll the entire viewport up by one row.
 * Row 0 is discarded; the new bottom row is filled with spaces.
 */
static void scroll_up(void)
{
    /* Move rows 1 … HEIGHT-1 up by one. */
    for (uint16_t row = 1; row < VGA_HEIGHT; row++) {
        for (uint16_t col = 0; col < VGA_WIDTH; col++) {
            terminal_buffer[(row - 1u) * VGA_WIDTH + col] =
                terminal_buffer[row * VGA_WIDTH + col];
        }
    }
    /* Clear the last row with the current colour. */
    for (uint16_t col = 0; col < VGA_WIDTH; col++) {
        put_entry(VGA_HEIGHT - 1u, col, ' ', terminal_color);
    }
    terminal_row = VGA_HEIGHT - 1u;
}

/* ── Public API ──────────────────────────────────────────────────────── */

void terminal_initialize(void)
{
    terminal_buffer = VGA_MEMORY;
    terminal_color  = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_row    = 0;
    terminal_col    = 0;
    terminal_clear();
}

void terminal_clear(void)
{
    for (uint16_t row = 0; row < VGA_HEIGHT; row++) {
        for (uint16_t col = 0; col < VGA_WIDTH; col++) {
            put_entry(row, col, ' ', terminal_color);
        }
    }
    terminal_row = 0;
    terminal_col = 0;
}

void terminal_setcolor(vga_color_t fg, vga_color_t bg)
{
    terminal_color = vga_entry_color(fg, bg);
}

void terminal_putchar(char c)
{
    switch (c) {
        case '\n':
            terminal_col = 0;
            terminal_row++;
            break;
        case '\r':
            terminal_col = 0;
            break;
        case '\t':
            /* Advance to the next 8-column tab stop. */
            terminal_col = (uint16_t)((terminal_col + 8u) & ~7u);
            if (terminal_col >= VGA_WIDTH) {
                terminal_col = 0;
                terminal_row++;
            }
            break;
        default:
            put_entry(terminal_row, terminal_col, c, terminal_color);
            terminal_col++;
            if (terminal_col >= VGA_WIDTH) {
                terminal_col = 0;
                terminal_row++;
            }
            break;
    }

    /* Scroll if we have gone past the last row. */
    if (terminal_row >= VGA_HEIGHT) {
        scroll_up();
    }
}

void terminal_writestring(const char* str)
{
    for (size_t i = 0; str[i] != '\0'; i++) {
        terminal_putchar(str[i]);
    }
}

void terminal_writestring_colored(const char* str, vga_color_t fg, vga_color_t bg)
{
    uint8_t saved = terminal_color;
    terminal_setcolor(fg, bg);
    terminal_writestring(str);
    terminal_color = saved;
}

void terminal_writehex32(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    char buf[11];   /* "0x" + 8 hex digits + '\0' */
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 9; i >= 2; i--) {
        buf[i] = hex[value & 0xFu];
        value >>= 4;
    }
    buf[10] = '\0';
    terminal_writestring(buf);
}
