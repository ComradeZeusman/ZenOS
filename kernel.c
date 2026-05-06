#include "kernel_types.h"
#include "gdt.h"
#include "idt.h"
#include "multiboot.h"
#include "pmm.h"

#define ERROR_STRING "\033[31m[ERROR]\033[0m "

// Function prototypes
void terminal_initialize(void);
void terminal_writestring(const char* str);
void terminal_writehex32(uint32_t value);
void terminal_putchar(char c);
void panic(const char* message);
void* kmalloc(size_t size);
void kfree(void* ptr);

// Ensure kernel_main is not removed by the linker
void kernel_main(uint32_t mb_magic, multiboot_info_t *mbi) __attribute__((used));

// Kernel entry point – called by boot.asm after GRUB multiboot handoff.
// mb_magic : must equal MULTIBOOT_BOOTLOADER_MAGIC (0x2BADB002)
// mbi      : pointer to the multiboot_info_t structure filled by GRUB
void kernel_main(uint32_t mb_magic, multiboot_info_t *mbi) {
    __asm__ volatile("cli");

    terminal_initialize();
    terminal_writestring("ZenOS Kernel Initializing...\n");

    if (mb_magic != MULTIBOOT_BOOTLOADER_MAGIC)
        panic("Not loaded by a multiboot-compliant bootloader");

    gdt_install();
    terminal_writestring("GDT installed\n");

    idt_install();
    terminal_writestring("IDT installed\n");

    pmm_init(mbi);
    terminal_writestring("PMM initialized  free=0x");
    terminal_writehex32(pmm_get_free_frames());
    terminal_writestring(" total=0x");
    terminal_writehex32(pmm_get_total_frames());
    terminal_writestring(" frames\n");

    __asm__ volatile("sti");

    for(;;) {
        __asm__ volatile("hlt");
    }
}

// Basic terminal output functions
static uint16_t* terminal_buffer;
static uint16_t terminal_row;
static uint16_t terminal_column;

void terminal_initialize() {
    terminal_buffer = (uint16_t*) 0xB8000;
    terminal_row = 0;
    terminal_column = 0;
    
    // Clear screen
    for (int i = 0; i < 80 * 25; i++) {
        terminal_buffer[i] = (uint16_t) ' ' | (uint16_t) 0x0F << 8;
    }
}

void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_column = 0;
        terminal_row++;
        return;
    }
    
    terminal_buffer[terminal_row * 80 + terminal_column] = (uint16_t) c | (uint16_t) 0x0F << 8;
    terminal_column++;
    
    if (terminal_column >= 80) {
        terminal_column = 0;
        terminal_row++;
    }
}

void terminal_writestring(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        terminal_putchar(str[i]);
    }
}

/* Print a 32-bit value as "0xXXXXXXXX" */
void terminal_writehex32(uint32_t value) {
    static const char hex[] = "0123456789ABCDEF";
    char buf[11];   /* "0x" + 8 hex digits + '\0' */
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 9; i >= 2; i--) {
        buf[i] = hex[value & 0xF];
        value >>= 4;
    }
    buf[10] = '\0';
    terminal_writestring(buf);
}

/*
 * kmalloc / kfree – page-granular wrappers over the PMM.
 * A higher-level heap slab allocator can be layered on top later.
 */
void* kmalloc(size_t size) {
    if (size == 0) return NULL;
    uint32_t addr = pmm_alloc_frame();
    if (addr == 0) {
        panic("kmalloc: out of physical memory");
        return NULL;
    }
    return (void*)addr;
}

void kfree(void* ptr) {
    if (!ptr) return;
    pmm_free_frame((uint32_t)ptr);
}

void panic(const char* message) {
    terminal_writestring(ERROR_STRING);
    terminal_writestring(message);
    terminal_writestring("\nSystem halted.\n");
    for(;;);
}