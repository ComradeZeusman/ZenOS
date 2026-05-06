#include "pic.h"
#include "port.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI   0x20   /* End-Of-Interrupt command */

/*
 * ICW1: start initialisation, expect ICW4.
 * ICW4: tell the PIC we are running in 8086/88 mode.
 */
#define ICW1_INIT 0x11
#define ICW4_8086 0x01

void pic_remap(void) {
    /* ICW1 – begin initialisation on both PICs */
    outb(PIC1_CMD,  ICW1_INIT); io_wait();
    outb(PIC2_CMD,  ICW1_INIT); io_wait();

    /* ICW2 – new vector base offsets */
    outb(PIC1_DATA, IRQ_BASE);  io_wait();   /* master: IRQ0 → INT 0x20 */
    outb(PIC2_DATA, IRQ_BASE2); io_wait();   /* slave:  IRQ8 → INT 0x28 */

    /* ICW3 – cascade wiring */
    outb(PIC1_DATA, 0x04); io_wait();   /* master: slave attached to IRQ2 */
    outb(PIC2_DATA, 0x02); io_wait();   /* slave: cascade identity = 2    */

    /* ICW4 – 8086 mode */
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    /* Mask all IRQ lines – unmask selectively when a handler is installed */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_send_eoi(uint8_t irq) {
    /* Slave PIC needs EOI too for IRQs 8-15 */
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

void pic_mask_all(void) {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t  mask;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    mask = inb(port) & (uint8_t)(~(1u << irq));
    outb(port, mask);
}
