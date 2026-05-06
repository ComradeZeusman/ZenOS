#ifndef TERMINAL_H
#define TERMINAL_H

#include "kernel_types.h"

/* ── VGA colour constants (4-bit palette) ────────────────────────────── */
typedef enum {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_YELLOW        = 14,
    VGA_COLOR_WHITE         = 15,
} vga_color_t;

/*
 * Pack a foreground + background colour pair into a VGA attribute byte.
 *   bits [3:0] – foreground colour
 *   bits [6:4] – background colour
 *   bit  7     – blink (left unused here)
 */
static inline uint8_t vga_entry_color(vga_color_t fg, vga_color_t bg) {
    return (uint8_t)fg | ((uint8_t)bg << 4);
}

/* Pack a character + attribute into a 16-bit VGA text-mode cell. */
static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
}

/* ── Public API ──────────────────────────────────────────────────────── */

/*
 * Set up the VGA text-mode driver.  Must be called before any other
 * terminal_* function.  Clears the screen with the default colour
 * (light grey on black).
 */
void terminal_initialize(void);

/*
 * Fill every cell with spaces using the current colour attribute.
 * Resets the cursor to (row 0, column 0).
 */
void terminal_clear(void);

/*
 * Change the active foreground/background colour.  All subsequent
 * terminal_putchar / terminal_writestring calls use the new colour.
 */
void terminal_setcolor(vga_color_t fg, vga_color_t bg);

/*
 * Write a single character.  Handles:
 *   '\n'  – move to the start of the next line
 *   '\r'  – carriage return (column → 0)
 *   '\t'  – advance to the next 8-column tab stop
 * When the cursor reaches the bottom row the screen scrolls up by one line.
 */
void terminal_putchar(char c);

/* Write a NUL-terminated string. */
void terminal_writestring(const char* str);

/* Write a NUL-terminated string in a specific colour, then restore the
   previous colour. */
void terminal_writestring_colored(const char* str, vga_color_t fg, vga_color_t bg);

/* Print a 32-bit value as "0xXXXXXXXX". */
void terminal_writehex32(uint32_t value);

#endif /* TERMINAL_H */
