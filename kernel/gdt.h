#ifndef GDT_H
#define GDT_H

#include "kernel_types.h"

/*
 * One 8-byte GDT segment descriptor.
 *
 * access byte layout (bit 7→0):
 *   P   – Present
 *   DPL – Descriptor Privilege Level (2 bits)
 *   S   – Descriptor type: 1 = code/data, 0 = system
 *   E   – Executable (1 = code segment)
 *   DC  – Direction/Conforming
 *   RW  – Readable (code) / Writable (data)
 *   A   – Accessed (CPU sets this; leave 0)
 *
 * granularity byte layout (bit 7→0):
 *   G   – Granularity: 1 = 4 KB pages
 *   DB  – 32-bit segment (1) / 16-bit (0)
 *   L   – 64-bit code segment (must be 0 for 32-bit)
 *   AVL – Available for OS use
 *   [3:0] – Upper 4 bits of 20-bit limit
 */
typedef struct {
    uint16_t limit_low;    /* bits 0-15 of segment limit  */
    uint16_t base_low;     /* bits 0-15 of base address   */
    uint8_t  base_middle;  /* bits 16-23 of base address  */
    uint8_t  access;       /* access / type flags         */
    uint8_t  granularity;  /* flags + upper limit nibble  */
    uint8_t  base_high;    /* bits 24-31 of base address  */
} __attribute__((packed)) gdt_entry_t;

/* Value loaded into GDTR with the LGDT instruction */
typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr_t;

/* Selector constants (byte offset into GDT) */
#define GDT_KERNEL_CODE_SEG  0x08
#define GDT_KERNEL_DATA_SEG  0x10

/* Public interface */
void gdt_install(void);
void gdt_set_gate(uint8_t num, uint32_t base, uint32_t limit,
                  uint8_t access, uint8_t granularity);

/* Defined in gdt_flush.asm – reloads GDTR and all segment registers */
extern void gdt_flush(uint32_t gdt_ptr_addr);

#endif /* GDT_H */
