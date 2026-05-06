#ifndef PORT_H
#define PORT_H

#include "kernel_types.h"

/* Write a byte to an I/O port */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* Read a byte from an I/O port */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*
 * A small delay by writing to port 0x80 (POST diagnostic port).
 * Required between consecutive PIC command bytes.
 */
static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif /* PORT_H */
