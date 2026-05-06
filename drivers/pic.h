#ifndef PIC_H
#define PIC_H

#include "kernel_types.h"

/*
 * After pic_remap():
 *   Master PIC IRQ 0-7  → INT vectors 0x20-0x27
 *   Slave  PIC IRQ 8-15 → INT vectors 0x28-0x2F
 */
#define IRQ_BASE  0x20
#define IRQ_BASE2 0x28

/* Remap both PICs and mask all IRQ lines. */
void pic_remap(void);

/*
 * Send End-Of-Interrupt to the PIC(s).
 * Must be called at the end of every hardware IRQ handler.
 * irq = 0-15 (raw IRQ line number, not the vector number).
 */
void pic_send_eoi(uint8_t irq);

/* Mask (disable) all IRQ lines on both PICs. */
void pic_mask_all(void);

/* Unmask (enable) a single IRQ line (0-15). */
void pic_unmask_irq(uint8_t irq);

/* Mask (disable) a single IRQ line (0-15). */
void pic_mask_irq(uint8_t irq);

/*
 * Spurious-IRQ detection.
 *
 * The 8259 can emit a spurious interrupt on IRQ7 (master) or IRQ15 (slave)
 * when noise causes a phantom pulse.  Read the In-Service Register (ISR)
 * before dispatching: if the relevant bit is clear the interrupt is spurious
 * and must be silently discarded (no EOI to the slave; master EOI still
 * required for IRQ15 because the cascade line was acknowledged).
 *
 * Returns non-zero (true) when the IRQ is spurious.
 */
int pic_is_spurious_irq7(void);
int pic_is_spurious_irq15(void);

#endif /* PIC_H */
