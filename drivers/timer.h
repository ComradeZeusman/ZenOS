#ifndef TIMER_H
#define TIMER_H

#include "kernel_types.h"

/*
 * Callback invoked on every timer tick, from inside the IRQ0 handler.
 * The future task scheduler registers itself here to implement preemption.
 * Only one callback is supported; pass NULL to unregister.
 *
 * IMPORTANT: this runs in interrupt context – keep it short and do not
 * call blocking functions.
 */
typedef void (*timer_callback_t)(void);

/*
 * timer_init(hz)
 *
 * Programs PIT channel 0 to fire at `hz` ticks per second, registers the
 * IRQ0 handler, and unmasks IRQ0 on the master PIC.
 * Must be called after idt_install() and before STI.
 */
void timer_init(uint32_t hz);

/*
 * timer_register_callback(cb)
 *
 * Register (or unregister with NULL) a function called on every tick.
 * Intended for the task scheduler; replaces any previously registered
 * callback.
 */
void timer_register_callback(timer_callback_t cb);

/*
 * timer_get_ticks()
 *
 * Returns the total number of timer ticks since timer_init() was called.
 * At 100 Hz this wraps after ~497 days (uint32_t). Use
 * timer_get_ticks64() when monotonic accuracy matters.
 */
uint32_t timer_get_ticks(void);

/*
 * timer_get_uptime_ms()
 *
 * Returns milliseconds elapsed since timer_init().
 * Resolution equals 1000/hz ms (10 ms at 100 Hz).
 */
uint32_t timer_get_uptime_ms(void);

/*
 * timer_sleep(ticks)
 *
 * Busy-waits (HLT loop) for at least `ticks` timer ticks.
 * Requires interrupts to be enabled (STI).
 */
void timer_sleep(uint32_t ticks);

/*
 * timer_sleep_ms(ms)
 *
 * Convenience wrapper: sleep for at least `ms` milliseconds.
 * Requires interrupts to be enabled (STI).
 */
void timer_sleep_ms(uint32_t ms);

#endif /* TIMER_H */
