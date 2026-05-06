#include "kernel_types.h"
#include "gdt.h"
#include "idt.h"
#include "multiboot.h"
#include "pmm.h"
#include "paging.h"
#include "terminal.h"
#include "keyboard.h"

// Function prototypes
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
    terminal_writestring_colored("ZenOS", VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_writestring(" Kernel Initializing...\n");

    if (mb_magic != MULTIBOOT_BOOTLOADER_MAGIC)
        panic("Not loaded by a multiboot-compliant bootloader");

    gdt_install();
    terminal_writestring_colored("[ OK ]", VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    terminal_writestring(" GDT installed\n");

    idt_install();
    terminal_writestring_colored("[ OK ]", VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    terminal_writestring(" IDT installed\n");

    pmm_init(mbi);
    terminal_writestring_colored("[ OK ]", VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    terminal_writestring(" PMM initialized  free=0x");
    terminal_writehex32(pmm_get_free_frames());
    terminal_writestring(" total=0x");
    terminal_writehex32(pmm_get_total_frames());
    terminal_writestring(" frames\n");

    paging_init();
    terminal_writestring_colored("[ OK ]", VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    terminal_writestring(" Paging enabled   identity-mapped first 4 MiB\n");

    keyboard_init();
    terminal_writestring_colored("[ OK ]", VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    terminal_writestring(" Keyboard driver ready\n");

    __asm__ volatile("sti");

    terminal_writestring("\n> ");

    for(;;) {
        __asm__ volatile("hlt");
        char c = keyboard_getchar();
        if (c) {
            if (c == '\b') {
                /* simple backspace: overwrite last char with space */
                terminal_writestring("\b \b");
            } else {
                terminal_putchar(c);
                if (c == '\n')
                    terminal_writestring("> ");
            }
        }
    }
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
    terminal_writestring_colored("[PANIC] ", VGA_COLOR_WHITE, VGA_COLOR_RED);
    terminal_writestring(message);
    terminal_writestring("\nSystem halted.\n");
    for(;;);
}