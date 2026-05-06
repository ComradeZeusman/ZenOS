#include "idt.h"
#include "pic.h"

#define IDT_ENTRIES 256

/* 0x8E = Present | DPL=0 | 32-bit interrupt gate */
#define IDT_FLAGS_KERNEL_INTERRUPT 0x8E

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t   idt_ptr;

static const char *exception_messages[32] = {
    "Division By Zero",           /*  0 #DE */
    "Debug",                      /*  1 #DB */
    "Non-Maskable Interrupt",     /*  2     */
    "Breakpoint",                 /*  3 #BP */
    "Overflow",                   /*  4 #OF */
    "Bound Range Exceeded",       /*  5 #BR */
    "Invalid Opcode",             /*  6 #UD */
    "Device Not Available",       /*  7 #NM */
    "Double Fault",               /*  8 #DF */
    "Coprocessor Segment Overrun",/*  9     */
    "Invalid TSS",                /* 10 #TS */
    "Segment Not Present",        /* 11 #NP */
    "Stack-Segment Fault",        /* 12 #SS */
    "General Protection Fault",   /* 13 #GP */
    "Page Fault",                 /* 14 #PF */
    "Reserved",                   /* 15     */
    "x87 FPU Error",              /* 16 #MF */
    "Alignment Check",            /* 17 #AC */
    "Machine Check",              /* 18 #MC */
    "SIMD Floating-Point Error",  /* 19 #XM */
    "Virtualization Exception",   /* 20 #VE */
    "Reserved",                   /* 21     */
    "Reserved",                   /* 22     */
    "Reserved",                   /* 23     */
    "Reserved",                   /* 24     */
    "Reserved",                   /* 25     */
    "Reserved",                   /* 26     */
    "Reserved",                   /* 27     */
    "Reserved",                   /* 28     */
    "Reserved",                   /* 29     */
    "Security Exception",         /* 30 #SX */
    "Reserved"                    /* 31     */
};

/* Forward declarations of kernel functions used here */
extern void terminal_writestring(const char *str);
extern void panic(const char *message);

void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags) {
    idt[num].base_low  = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector  = selector;
    idt[num].zero      = 0;
    idt[num].flags     = flags;
}

void idt_install(void) {
    idt_ptr.limit = (uint16_t)(sizeof(idt_entry_t) * IDT_ENTRIES - 1);
    idt_ptr.base  = (uint32_t)&idt;

    /*
     * Remap the 8259 PIC so that hardware IRQs 0-15 are delivered on
     * vectors 0x20-0x2F instead of the default 0x08-0x0F/0x70-0x77.
     * The default mapping overlaps the CPU exception range (0x00-0x1F),
     * which would cause spurious Double Faults (e.g. timer IRQ0 → vector 8).
     * All IRQ lines are masked inside pic_remap; unmask selectively later.
     */
    pic_remap();

    /* Zero all entries first */
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate((uint8_t)i, 0, 0, 0);
    }

    /* Install CPU exception handlers (ISRs 0-31) */
    idt_set_gate( 0, (uint32_t)isr0,  0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate( 1, (uint32_t)isr1,  0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate( 2, (uint32_t)isr2,  0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate( 3, (uint32_t)isr3,  0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate( 4, (uint32_t)isr4,  0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate( 5, (uint32_t)isr5,  0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate( 6, (uint32_t)isr6,  0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate( 7, (uint32_t)isr7,  0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate( 8, (uint32_t)isr8,  0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate( 9, (uint32_t)isr9,  0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(10, (uint32_t)isr10, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(11, (uint32_t)isr11, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(12, (uint32_t)isr12, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(13, (uint32_t)isr13, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(14, (uint32_t)isr14, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(15, (uint32_t)isr15, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(16, (uint32_t)isr16, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(17, (uint32_t)isr17, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(18, (uint32_t)isr18, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(19, (uint32_t)isr19, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(20, (uint32_t)isr20, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(21, (uint32_t)isr21, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(22, (uint32_t)isr22, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(23, (uint32_t)isr23, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(24, (uint32_t)isr24, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(25, (uint32_t)isr25, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(26, (uint32_t)isr26, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(27, (uint32_t)isr27, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(28, (uint32_t)isr28, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(29, (uint32_t)isr29, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(30, (uint32_t)isr30, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(31, (uint32_t)isr31, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);

    /* Hardware IRQ handlers (vectors 0x20-0x2F, IRQs 0-15) */
    idt_set_gate(32, (uint32_t)isr32, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(33, (uint32_t)isr33, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(34, (uint32_t)isr34, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(35, (uint32_t)isr35, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(36, (uint32_t)isr36, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(37, (uint32_t)isr37, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(38, (uint32_t)isr38, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(39, (uint32_t)isr39, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(40, (uint32_t)isr40, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(41, (uint32_t)isr41, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(42, (uint32_t)isr42, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(43, (uint32_t)isr43, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(44, (uint32_t)isr44, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(45, (uint32_t)isr45, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(46, (uint32_t)isr46, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);
    idt_set_gate(47, (uint32_t)isr47, 0x08, IDT_FLAGS_KERNEL_INTERRUPT);

    /* Load the IDT register */
    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
}

void isr_handler(registers_t *regs) {
    if (regs->int_no < 32) {
        /* CPU exception */
        terminal_writestring("EXCEPTION: ");
        terminal_writestring(exception_messages[regs->int_no]);
        terminal_writestring("\n");
        panic("Unhandled CPU exception - system halted");
    } else if (regs->int_no < 48) {
        /*
         * Hardware IRQ (vectors 0x20-0x2F).
         * All IRQ lines are masked after pic_remap(), so we only get here
         * if a specific IRQ has been unmasked.  Send EOI so the PIC
         * accepts future interrupts on this line.
         */
        pic_send_eoi((uint8_t)(regs->int_no - 32));
    }
}
