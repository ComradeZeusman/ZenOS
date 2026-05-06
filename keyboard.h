#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "kernel_types.h"

/*
 * keyboard_init()
 *
 * Registers the IRQ1 handler and unmasks IRQ1 on the PIC.
 * Must be called after idt_install() and with interrupts still disabled;
 * the IDT is live but STI has not been issued yet.
 * After this call, keyboard interrupts will fire as soon as STI executes.
 */
void keyboard_init(void);

/*
 * keyboard_getchar()
 *
 * Non-blocking read from the internal ring buffer.
 * Returns the next ASCII character if one is available, or 0 if the
 * buffer is empty.  Special/modifier keys (Shift, Ctrl, Alt, F-keys …)
 * do not produce a character and are silently consumed.
 */
char keyboard_getchar(void);

/*
 * keyboard_getchar_blocking()
 *
 * Spins (HLT loop) until a character is available, then returns it.
 * Requires interrupts to be enabled (STI).
 */
char keyboard_getchar_blocking(void);

#endif /* KEYBOARD_H */
