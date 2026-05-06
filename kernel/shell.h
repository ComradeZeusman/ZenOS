#ifndef SHELL_H
#define SHELL_H

/*
 * shell_run()
 *
 * Enters the interactive kernel shell loop (never returns).
 * Reads characters from the keyboard, accumulates them into a line buffer,
 * and dispatches to a built-in command handler on Enter.
 *
 * Requires interrupts to be enabled (STI) and the keyboard driver to be
 * initialised before calling.
 */
void shell_run(void);

#endif /* SHELL_H */
