#include "kernel_types.h"
#include "gdt.h"
#include "idt.h"
#include "multiboot.h"
#include "pmm.h"
#include "paging.h"
#include "terminal.h"
#include "keyboard.h"
#include "timer.h"
#include "shell.h"
#include "klog.h"

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
    KLOG_INFO("GDT installed");

    idt_install();
    KLOG_INFO("IDT installed");

    pmm_init(mbi);
    KLOG_INFO("PMM initialized");
    klog_hex(LOG_DEBUG, "  free  frames: ", pmm_get_free_frames());
    klog_hex(LOG_DEBUG, "  total frames: ", pmm_get_total_frames());

    paging_init();
    KLOG_INFO("Paging enabled  (identity-mapped first 4 MiB)");

    keyboard_init();
    KLOG_INFO("Keyboard driver ready");

    timer_init(100);
    KLOG_INFO("PIT timer running @ 100 Hz");

    __asm__ volatile("sti");

    shell_run();   /* never returns */
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
    __asm__ volatile("cli");   /* disable interrupts – we are not coming back */
    terminal_setcolor(VGA_COLOR_WHITE, VGA_COLOR_RED);
    terminal_writestring("\n[PANIC] ");
    terminal_writestring(message);
    terminal_writestring("\nSystem halted.\n");
    for (;;) {
        __asm__ volatile("hlt");   /* power-efficient halt */
    }
}