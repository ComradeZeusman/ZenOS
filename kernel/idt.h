#ifndef IDT_H
#define IDT_H

#include "kernel_types.h"

/* One 8-byte IDT gate descriptor */
typedef struct {
    uint16_t base_low;   /* bits 0-15 of the handler address  */
    uint16_t selector;   /* kernel code segment selector       */
    uint8_t  zero;       /* always 0                           */
    uint8_t  flags;      /* type + DPL + present bit           */
    uint16_t base_high;  /* bits 16-31 of the handler address  */
} __attribute__((packed)) idt_entry_t;

/* Value loaded into IDTR with the LIDT instruction */
typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

/*
 * Snapshot of the CPU state captured by isr_common_stub.
 * Layout must match the exact push order in isr.asm:
 *   ds, edi, esi, ebp, esp, ebx, edx, ecx, eax  (saved by us)
 *   int_no, err_code                              (saved by us)
 *   eip, cs, eflags                               (saved by CPU)
 */
typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; /* pusha order (top→bottom) */
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;                         /* pushed by the CPU        */
} registers_t;

/* Public interface */
void idt_install(void);
void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags);
void isr_handler(registers_t *regs);

/* ISR entry-points defined in isr.asm */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

/* Hardware IRQ entry-points (vectors 0x20-0x2F) */
extern void isr32(void);  extern void isr33(void);  extern void isr34(void);
extern void isr35(void);  extern void isr36(void);  extern void isr37(void);
extern void isr38(void);  extern void isr39(void);  extern void isr40(void);
extern void isr41(void);  extern void isr42(void);  extern void isr43(void);
extern void isr44(void);  extern void isr45(void);  extern void isr46(void);
extern void isr47(void);

/*
 * IRQ handler registration.
 * Drivers call irq_register_handler(irq_line, fn) to receive a callback
 * whenever the corresponding hardware IRQ fires (irq = 0-15).
 * Pass NULL to unregister.
 */
typedef void (*irq_handler_t)(registers_t *regs);
void irq_register_handler(uint8_t irq, irq_handler_t handler);

#endif /* IDT_H */
