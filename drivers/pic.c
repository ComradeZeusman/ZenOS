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

void pic_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t  mask;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    mask = inb(port) | (uint8_t)(1u << irq);
    outb(port, mask);
}

/*
 * Read the In-Service Register of one PIC.
 * Writing OCW3 with bit 1 set (0x0B) requests that the next read from the
 * command port returns the ISR rather than the IRR.
 * A set bit means that IRQ is currently being serviced; a clear bit for the
 * highest-priority pending line means the interrupt was spurious.
 */
static uint8_t pic_read_isr(uint16_t cmd_port) {
    outb(cmd_port, 0x0B);   /* OCW3: read ISR on next read */
    return inb(cmd_port);
}

int pic_is_spurious_irq7(void) {
    /* Spurious if master ISR bit 7 is clear */
    return !(pic_read_isr(PIC1_CMD) & 0x80u);
}

int pic_is_spurious_irq15(void) {
    /* Spurious if slave ISR bit 7 is clear */
    if (!(pic_read_isr(PIC2_CMD) & 0x80u)) {
        /*
         * The master already acknowledged the cascade (IRQ2) pulse, so we
         * must still send EOI to the master — but NOT to the slave, because
         * the slave never actually raised an interrupt.
         */
        outb(PIC1_CMD, PIC_EOI);
        return 1;
    }
    return 0;
}
