#include "keyboard.h"
#include "idt.h"
#include "pic.h"
#include "port.h"

/* ── Hardware constants ───────────────────────────────────────────────── */
#define KB_DATA_PORT    0x60    /* read scancode / send command             */
#define KB_STATUS_PORT  0x64    /* bit 0 = output buffer full (data ready)  */
#define SCANCODE_RELEASE_BIT 0x80  /* set in a scancode when the key is released */

/* ── Scancode Set 1 → ASCII tables ──────────────────────────────────── */
/*
 * Index = Set-1 make code (0x01 – 0x58).
 * 0 means "no printable character" (modifier, function key, etc.).
 */
static const char sc_ascii_lower[89] = {
/*00*/  0,
/*01*/  0,     /* Escape        */
/*02*/ '1',
/*03*/ '2',
/*04*/ '3',
/*05*/ '4',
/*06*/ '5',
/*07*/ '6',
/*08*/ '7',
/*09*/ '8',
/*0A*/ '9',
/*0B*/ '0',
/*0C*/ '-',
/*0D*/ '=',
/*0E*/ '\b',   /* Backspace     */
/*0F*/ '\t',   /* Tab           */
/*10*/ 'q',
/*11*/ 'w',
/*12*/ 'e',
/*13*/ 'r',
/*14*/ 't',
/*15*/ 'y',
/*16*/ 'u',
/*17*/ 'i',
/*18*/ 'o',
/*19*/ 'p',
/*1A*/ '[',
/*1B*/ ']',
/*1C*/ '\n',   /* Enter         */
/*1D*/ 0,      /* Left Ctrl     */
/*1E*/ 'a',
/*1F*/ 's',
/*20*/ 'd',
/*21*/ 'f',
/*22*/ 'g',
/*23*/ 'h',
/*24*/ 'j',
/*25*/ 'k',
/*26*/ 'l',
/*27*/ ';',
/*28*/ '\'',
/*29*/ '`',
/*2A*/ 0,      /* Left Shift    */
/*2B*/ '\\',
/*2C*/ 'z',
/*2D*/ 'x',
/*2E*/ 'c',
/*2F*/ 'v',
/*30*/ 'b',
/*31*/ 'n',
/*32*/ 'm',
/*33*/ ',',
/*34*/ '.',
/*35*/ '/',
/*36*/ 0,      /* Right Shift   */
/*37*/ '*',    /* Keypad *      */
/*38*/ 0,      /* Left Alt      */
/*39*/ ' ',
/*3A*/ 0,      /* Caps Lock     */
/*3B*/ 0, /*3C*/ 0, /*3D*/ 0, /*3E*/ 0, /*3F*/ 0, /* F1-F5         */
/*40*/ 0, /*41*/ 0, /*42*/ 0, /*43*/ 0, /*44*/ 0, /* F6-F10        */
/*45*/ 0,      /* Num Lock      */
/*46*/ 0,      /* Scroll Lock   */
/*47*/ '7', /*48*/ '8', /*49*/ '9', /*4A*/ '-', /* Keypad        */
/*4B*/ '4', /*4C*/ '5', /*4D*/ '6', /*4E*/ '+', /* Keypad        */
/*4F*/ '1', /*50*/ '2', /*51*/ '3', /*52*/ '0', /* Keypad        */
/*53*/ '.',    /* Keypad .      */
/*54*/ 0, /*55*/ 0, /*56*/ 0,
/*57*/ 0, /*58*/ 0                              /* F11, F12       */
};

static const char sc_ascii_upper[89] = {
/*00*/  0,
/*01*/  0,
/*02*/ '!', /*03*/ '@', /*04*/ '#', /*05*/ '$', /*06*/ '%',
/*07*/ '^', /*08*/ '&', /*09*/ '*', /*0A*/ '(', /*0B*/ ')',
/*0C*/ '_', /*0D*/ '+',
/*0E*/ '\b',
/*0F*/ '\t',
/*10*/ 'Q', /*11*/ 'W', /*12*/ 'E', /*13*/ 'R', /*14*/ 'T',
/*15*/ 'Y', /*16*/ 'U', /*17*/ 'I', /*18*/ 'O', /*19*/ 'P',
/*1A*/ '{', /*1B*/ '}',
/*1C*/ '\n',
/*1D*/ 0,
/*1E*/ 'A', /*1F*/ 'S', /*20*/ 'D', /*21*/ 'F', /*22*/ 'G',
/*23*/ 'H', /*24*/ 'J', /*25*/ 'K', /*26*/ 'L',
/*27*/ ':', /*28*/ '"', /*29*/ '~',
/*2A*/ 0,
/*2B*/ '|',
/*2C*/ 'Z', /*2D*/ 'X', /*2E*/ 'C', /*2F*/ 'V', /*30*/ 'B',
/*31*/ 'N', /*32*/ 'M',
/*33*/ '<', /*34*/ '>', /*35*/ '?',
/*36*/ 0,
/*37*/ '*',
/*38*/ 0,
/*39*/ ' ',
/*3A*/ 0,
/*3B*/ 0, /*3C*/ 0, /*3D*/ 0, /*3E*/ 0, /*3F*/ 0,
/*40*/ 0, /*41*/ 0, /*42*/ 0, /*43*/ 0, /*44*/ 0,
/*45*/ 0, /*46*/ 0,
/*47*/ '7', /*48*/ '8', /*49*/ '9', /*4A*/ '-',
/*4B*/ '4', /*4C*/ '5', /*4D*/ '6', /*4E*/ '+',
/*4F*/ '1', /*50*/ '2', /*51*/ '3', /*52*/ '0',
/*53*/ '.',
/*54*/ 0, /*55*/ 0, /*56*/ 0,
/*57*/ 0, /*58*/ 0
};

/* ── Ring buffer ─────────────────────────────────────────────────────── */
#define KB_BUFFER_SIZE 256u

static volatile char kb_buffer[KB_BUFFER_SIZE];
static volatile uint32_t kb_buf_head = 0;   /* write index (IRQ side)  */
static volatile uint32_t kb_buf_tail = 0;   /* read  index (caller side) */

static inline void kb_buffer_push(char c) {
    uint32_t next = (kb_buf_head + 1u) & (KB_BUFFER_SIZE - 1u);
    if (next != kb_buf_tail) {   /* drop if full */
        kb_buffer[kb_buf_head] = c;
        kb_buf_head = next;
    }
}

/* ── Modifier state ──────────────────────────────────────────────────── */
static volatile uint8_t shift_held = 0;   /* non-zero while Shift is down */

/* ── IRQ1 handler ────────────────────────────────────────────────────── */
static void keyboard_irq_handler(registers_t *regs) {
    (void)regs;

    /* Only read if the output buffer is actually full. */
    if (!(inb(KB_STATUS_PORT) & 0x01))
        return;

    uint8_t scancode = inb(KB_DATA_PORT);

    if (scancode & SCANCODE_RELEASE_BIT) {
        /* Key-release event */
        uint8_t sc = scancode & ~SCANCODE_RELEASE_BIT;
        if (sc == 0x2A || sc == 0x36)   /* Left / Right Shift */
            shift_held = 0;
        return;
    }

    /* Key-press event */
    if (scancode == 0x2A || scancode == 0x36) {
        shift_held = 1;
        return;
    }

    if (scancode < 89) {
        char c = shift_held ? sc_ascii_upper[scancode]
                            : sc_ascii_lower[scancode];
        if (c)
            kb_buffer_push(c);
    }
}

/* ── Public API ──────────────────────────────────────────────────────── */

void keyboard_init(void) {
    irq_register_handler(1, keyboard_irq_handler);
    pic_unmask_irq(1);
}

char keyboard_getchar(void) {
    if (kb_buf_tail == kb_buf_head)
        return 0;
    char c = kb_buffer[kb_buf_tail];
    kb_buf_tail = (kb_buf_tail + 1u) & (KB_BUFFER_SIZE - 1u);
    return c;
}

char keyboard_getchar_blocking(void) {
    char c;
    while ((c = keyboard_getchar()) == 0)
        __asm__ volatile("hlt");
    return c;
}
