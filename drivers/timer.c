#include "timer.h"
#include "idt.h"
#include "pic.h"
#include "port.h"

/* ── PIT (Intel 8253/8254) register map ─────────────────────────────── */
#define PIT_CHANNEL0    0x40    /* Channel 0 data port (read/write)       */
#define PIT_CMD         0x43    /* Mode/command register (write only)      */

/*
 * Command byte: channel 0 | lobyte/hibyte access | mode 2 (rate generator)
 * | binary counting.
 *   bits 7-6 : 00  – channel 0
 *   bits 5-4 : 11  – access mode: lobyte then hibyte
 *   bits 3-1 : 010 – operating mode 2 (rate generator / divide-by-N)
 *   bit  0   :  0  – binary (not BCD)
 */
#define PIT_CMD_RATE    0x34

/* PIT oscillator frequency in Hz (approximately 1.193182 MHz) */
#define PIT_BASE_HZ     1193182u

/* ── Internal state ─────────────────────────────────────────────────── */

/* Tick frequency configured by timer_init(); stored for ms conversion. */
static uint32_t timer_hz = 100;

/*
 * Monotonic tick counter.  Incremented in IRQ context on every PIT tick.
 * uint32_t wraps ~497 days at 100 Hz which is fine for early kernel use.
 */
static volatile uint32_t tick_count = 0;

/*
 * Optional scheduler hook.  Called from IRQ0 on every tick, before EOI.
 * NULL until a scheduler registers itself via timer_register_callback().
 */
static timer_callback_t tick_callback = ((void*)0);

/* ── IRQ0 handler ───────────────────────────────────────────────────── */
static void timer_irq_handler(registers_t *regs) {
    (void)regs;

    tick_count++;

    /*
     * Invoke the scheduler (or any other per-tick hook) if registered.
     * This fires before the EOI is sent by isr_handler(), which is fine:
     * the PIC EOI for IRQ0 is sent by the common IRQ dispatcher in idt.c
     * immediately after this handler returns.
     */
    if (tick_callback)
        tick_callback();
}

/* ── Public API ─────────────────────────────────────────────────────── */

void timer_init(uint32_t hz) {
    if (hz == 0) hz = 100;
    timer_hz = hz;

    /*
     * Reload value = PIT_BASE_HZ / hz.
     * At 100 Hz: divisor = 11931 (actual ~100.00 Hz).
     * Clamped to 16 bits; a divisor of 0 means 65536 (~18.2 Hz).
     */
    uint32_t divisor = PIT_BASE_HZ / hz;
    if (divisor > 0xFFFF) divisor = 0xFFFF;

    /* Send command byte, then reload value low byte, then high byte */
    outb(PIT_CMD,      PIT_CMD_RATE);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    irq_register_handler(0, timer_irq_handler);
    pic_unmask_irq(0);
}

void timer_register_callback(timer_callback_t cb) {
    tick_callback = cb;
}

uint32_t timer_get_ticks(void) {
    return tick_count;
}

uint32_t timer_get_uptime_ms(void) {
    /* Multiply before divide to preserve precision. */
    return (tick_count * 1000u) / timer_hz;
}

void timer_sleep(uint32_t ticks) {
    uint32_t end = tick_count + ticks;
    while (tick_count < end)
        __asm__ volatile("hlt");
}

void timer_sleep_ms(uint32_t ms) {
    /*
     * Convert ms → ticks, rounding up so we always sleep at least `ms`.
     * At 100 Hz one tick = 10 ms; timer_sleep_ms(25) waits 3 ticks (30 ms).
     */
    uint32_t ticks = (ms * timer_hz + 999u) / 1000u;
    timer_sleep(ticks);
}
