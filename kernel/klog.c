#include "klog.h"
#include "terminal.h"

typedef struct {
    const char  *tag;
    vga_color_t  fg;
    vga_color_t  bg;
} level_desc_t;

static const level_desc_t level_desc[4] = {
    { "[DEBUG] ", VGA_COLOR_DARK_GREY,   VGA_COLOR_BLACK }, /* LOG_DEBUG */
    { "[ INFO] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK }, /* LOG_INFO  */
    { "[ WARN] ", VGA_COLOR_YELLOW,      VGA_COLOR_BLACK }, /* LOG_WARN  */
    { "[ERROR] ", VGA_COLOR_LIGHT_RED,   VGA_COLOR_BLACK }, /* LOG_ERROR */
};

void klog(log_level_t level, const char *msg)
{
    if ((unsigned)level >= 4u)
        level = LOG_ERROR;

    terminal_writestring_colored(level_desc[level].tag,
                                 level_desc[level].fg,
                                 level_desc[level].bg);
    terminal_writestring(msg);
    terminal_putchar('\n');
}

void klog_hex(log_level_t level, const char *prefix, uint32_t val)
{
    if ((unsigned)level >= 4u)
        level = LOG_ERROR;

    terminal_writestring_colored(level_desc[level].tag,
                                 level_desc[level].fg,
                                 level_desc[level].bg);
    terminal_writestring(prefix);
    terminal_writehex32(val);
    terminal_putchar('\n');
}
