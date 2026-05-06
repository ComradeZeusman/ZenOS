#include "shell.h"
#include "terminal.h"
#include "keyboard.h"
#include "timer.h"
#include "pmm.h"

/* ── Configuration ───────────────────────────────────────────────────── */
#define CMD_BUF_SIZE 128u   /* maximum input line length (incl. NUL)      */
#define PROMPT       "> "

/* ── Internal helpers ────────────────────────────────────────────────── */

static int kstrcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

/* Print a decimal uint32_t without any C library. */
static void print_uint32(uint32_t v) {
    char buf[11];   /* max 10 digits + NUL */
    int  i = 10;
    buf[i] = '\0';
    if (v == 0) {
        terminal_putchar('0');
        return;
    }
    while (v > 0) {
        buf[--i] = (char)('0' + (v % 10));
        v /= 10;
    }
    terminal_writestring(&buf[i]);
}

/* ── Built-in command handlers ───────────────────────────────────────── */

static void cmd_help(void) {
    terminal_writestring_colored("ZenOS Shell — built-in commands\n", VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_writestring("  help    — show this help message\n");
    terminal_writestring("  clear   — clear the screen\n");
    terminal_writestring("  uptime  — seconds elapsed since boot\n");
    terminal_writestring("  mem     — free / total physical memory frames\n");
    terminal_writestring("  reboot  — reboot the machine\n");
}

static void cmd_clear(void) {
    terminal_clear();
}

static void cmd_uptime(void) {
    uint32_t ms = timer_get_uptime_ms();
    terminal_writestring("Uptime: ");
    print_uint32(ms / 1000u);
    terminal_putchar('.');
    /* one decimal place (tenths of a second) */
    print_uint32((ms % 1000u) / 100u);
    terminal_writestring(" s\n");
}

static void cmd_mem(void) {
    terminal_writestring("Memory: free=");
    print_uint32(pmm_get_free_frames());
    terminal_writestring(" frames  total=");
    print_uint32(pmm_get_total_frames());
    terminal_writestring(" frames  (1 frame = 4 KiB)\n");
}

static void cmd_reboot(void) {
    terminal_writestring_colored("Rebooting...\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);

    /*
     * Triple-fault reboot: load an IDT with a zero limit, then execute INT3.
     * The CPU cannot handle the exception → triple fault → BIOS/firmware reset.
     * This is the standard approach when ACPI/keyboard-controller reset is not
     * yet implemented.
     */
    struct { uint16_t limit; uint32_t base; } __attribute__((packed)) null_idt = {0, 0};
    __asm__ volatile(
        "lidt %0\n\t"
        "int $3\n\t"
        : : "m"(null_idt)
    );

    /* Should never reach here, but prevent the compiler from falling through */
    for (;;) __asm__ volatile("hlt");
}

/* ── Command dispatch ────────────────────────────────────────────────── */

typedef struct {
    const char *name;
    void (*fn)(void);
} shell_cmd_t;

static const shell_cmd_t commands[] = {
    { "help",   cmd_help   },
    { "clear",  cmd_clear  },
    { "uptime", cmd_uptime },
    { "mem",    cmd_mem    },
    { "reboot", cmd_reboot },
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

static void dispatch(const char *line) {
    /* Skip leading spaces */
    while (*line == ' ') line++;

    /* Empty line — just re-print the prompt */
    if (*line == '\0') return;

    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (kstrcmp(line, commands[i].name) == 0) {
            commands[i].fn();
            return;
        }
    }

    terminal_writestring_colored("Unknown command: ", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal_writestring(line);
    terminal_writestring("  (type 'help' for a list)\n");
}

/* ── Shell main loop ─────────────────────────────────────────────────── */

void shell_run(void) {
    static char buf[CMD_BUF_SIZE];
    size_t len = 0;

    terminal_writestring_colored("\nZenOS", VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_writestring(" shell ready.  Type 'help' for commands.\n\n");
    terminal_writestring(PROMPT);

    for (;;) {
        __asm__ volatile("hlt");   /* sleep until next interrupt */

        char c = keyboard_getchar();
        if (!c) continue;

        if (c == '\b') {
            /* Backspace: erase last character if buffer non-empty */
            if (len > 0) {
                len--;
                terminal_writestring("\b \b");
            }
        } else if (c == '\n') {
            terminal_putchar('\n');
            buf[len] = '\0';
            dispatch(buf);
            len = 0;
            terminal_writestring(PROMPT);
        } else {
            /* Printable character — append to buffer if space remains */
            if (len < CMD_BUF_SIZE - 1u) {
                buf[len++] = c;
                terminal_putchar(c);
            }
        }
    }
}
