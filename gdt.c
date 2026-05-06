#include "gdt.h"

/*
 * Flat 32-bit GDT layout
 * ----------------------
 * Index 0 (0x00) – Null descriptor      (required by the CPU spec)
 * Index 1 (0x08) – Kernel code segment  (ring 0, execute/read, 4 GB flat)
 * Index 2 (0x10) – Kernel data segment  (ring 0, read/write,   4 GB flat)
 *
 * Access byte values:
 *   0x9A = 1001 1010b  P=1 DPL=00 S=1 E=1 DC=0 RW=1 A=0  (code, readable)
 *   0x92 = 1001 0010b  P=1 DPL=00 S=1 E=0 DC=0 RW=1 A=0  (data, writable)
 *
 * Granularity byte:
 *   0xCF = 1100 1111b  G=1 DB=1 L=0 AVL=0 limit[19:16]=1111
 *          ^^                                ^^^^
 *          4 KB granularity, 32-bit         upper limit nibble 0xF
 *
 * With G=1 and limit=0xFFFFF the accessible range is 0x00000000–0xFFFFFFFF.
 */

#define GDT_SIZE 3

static gdt_entry_t gdt[GDT_SIZE];
static gdt_ptr_t   gdt_ptr;

void gdt_set_gate(uint8_t num, uint32_t base, uint32_t limit,
                  uint8_t access, uint8_t granularity)
{
    gdt[num].base_low    =  base        & 0xFFFF;
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;

    gdt[num].limit_low   =  limit       & 0xFFFF;
    /* merge upper 4 bits of limit into top nibble of granularity byte */
    gdt[num].granularity = (granularity & 0xF0) | ((limit >> 16) & 0x0F);

    gdt[num].access      = access;
}

void gdt_install(void)
{
    gdt_ptr.limit = (uint16_t)(sizeof(gdt_entry_t) * GDT_SIZE - 1);
    gdt_ptr.base  = (uint32_t)&gdt;

    /* Descriptor 0: null – all fields zero (CPU requirement) */
    gdt_set_gate(0, 0, 0, 0, 0);

    /* Descriptor 1: kernel code  selector = 0x08 */
    gdt_set_gate(1,
                 0x00000000,   /* base  = 0 (flat)          */
                 0x000FFFFF,   /* limit = 0xFFFFF pages      */
                 0x9A,         /* P DPL=0 S=1 E=1 RW=1      */
                 0xC0);        /* G=1 DB=1 + upper limit=0xF */

    /* Descriptor 2: kernel data  selector = 0x10 */
    gdt_set_gate(2,
                 0x00000000,
                 0x000FFFFF,
                 0x92,         /* P DPL=0 S=1 E=0 RW=1      */
                 0xC0);

    /* Atomically load the new GDTR and flush all segment registers */
    gdt_flush((uint32_t)&gdt_ptr);
}
