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

#endif /* PIC_H */
