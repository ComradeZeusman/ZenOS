#include "kernel_types.h"
#include "gdt.h"
#include "idt.h"
#include "multiboot.h"

#define MEMORY_END 0x1000000  // 16MB of memory
#define PAGE_SIZE 4096
#define ERROR_STRING "\033[31m[ERROR]\033[0m "

// Function prototypes
void terminal_initialize(void);
void terminal_writestring(const char* str);
void terminal_writehex32(uint32_t value);
void terminal_putchar(char c);
void init_memory(multiboot_info_t *mbi);
void panic(const char* message);
void* kmalloc(size_t size);
void kfree(void* ptr);

/* Linker-exported end-of-kernel symbol (defined in linker.ld) */
extern uint32_t _kernel_end;

// Ensure kernel_main is not removed by the linker
void kernel_main(uint32_t mb_magic, multiboot_info_t *mbi) __attribute__((used));

// Memory management structures
typedef struct {
    uint32_t present : 1;
    uint32_t rw : 1;
    uint32_t user : 1;
    uint32_t accessed : 1;
    uint32_t dirty : 1;
    uint32_t unused : 7;
    uint32_t frame : 20;
} page_t;

typedef struct {
    page_t pages[1024];
} page_table_t;

typedef struct {
    page_table_t *tables[1024];
    uint32_t physical_tables[1024];
    uint32_t physical_address;
} page_directory_t;

// Additional memory management structures
typedef struct {
    uint32_t start_address;
    uint32_t size;
    uint32_t used : 1;
} memory_block_t;

#define MAX_BLOCKS 1024
static memory_block_t memory_blocks[MAX_BLOCKS];
static uint32_t num_blocks = 0;

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

    init_memory(mbi);
    terminal_writestring("Memory management initialized\n");

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

// Memory management implementation
void init_memory(multiboot_info_t *mbi) {
    num_blocks = 0;
    terminal_writestring("Memory map:\n");

    if (mbi->flags & MULTIBOOT_INFO_MMAP) {
        /*
         * Parse the BIOS memory map provided by GRUB.
         * We only add page-aligned available regions above 1 MB
         * and above the kernel image itself to the allocator.
         */
        uint32_t kend = ((uint32_t)&_kernel_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        multiboot_mmap_entry_t *entry   = (multiboot_mmap_entry_t*)(mbi->mmap_addr);
        multiboot_mmap_entry_t *map_end = (multiboot_mmap_entry_t*)(mbi->mmap_addr + mbi->mmap_length);

        while (entry < map_end) {
            /* Print the raw BIOS region */
            terminal_writestring("  ");
            terminal_writehex32(entry->base_low);
            terminal_writestring(" - ");
            terminal_writehex32(entry->base_low + entry->length_low - 1);
            terminal_writestring(entry->type == MULTIBOOT_MEMORY_AVAILABLE
                                 ? " [Available]\n" : " [Reserved]\n");

            if (entry->type == MULTIBOOT_MEMORY_AVAILABLE && num_blocks < MAX_BLOCKS) {
                uint32_t base = entry->base_low;
                uint32_t size = entry->length_low;
                int valid = 1;

                /* Skip / trim everything below 1 MB */
                if (base + size <= 0x100000) {
                    valid = 0;
                } else if (base < 0x100000) {
                    size -= (0x100000 - base);
                    base  = 0x100000;
                }

                /* Skip / trim the kernel image */
                if (valid && base + size <= kend) {
                    valid = 0;
                } else if (valid && base < kend) {
                    size -= (kend - base);
                    base  = kend;
                }

                if (valid) {
                    /* Align base up and size down to page boundaries */
                    uint32_t aligned = (base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
                    uint32_t trim    = aligned - base;
                    if (size > trim) {
                        size -= trim;
                        base  = aligned;
                        size &= ~(PAGE_SIZE - 1);
                        if (size > 0) {
                            memory_blocks[num_blocks].start_address = base;
                            memory_blocks[num_blocks].size          = size;
                            memory_blocks[num_blocks].used          = 0;
                            num_blocks++;
                        }
                    }
                }
            }

            entry = (multiboot_mmap_entry_t*)
                    ((uint32_t)entry + entry->size + (uint32_t)sizeof(entry->size));
        }
    } else if (mbi->flags & MULTIBOOT_INFO_MEM) {
        /* Fallback: mem_upper = KB of RAM above 1 MB */
        terminal_writestring("  (mem_upper fallback)\n");
        memory_blocks[0].start_address = 0x100000;
        memory_blocks[0].size          = mbi->mem_upper * 1024;
        memory_blocks[0].used          = 0;
        num_blocks = 1;
    } else {
        /* Last resort: hardcoded 16 MB */
        terminal_writestring("  (hardcoded fallback)\n");
        memory_blocks[0].start_address = 0x100000;
        memory_blocks[0].size          = MEMORY_END - 0x100000;
        memory_blocks[0].used          = 0;
        num_blocks = 1;
    }
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    // Align size to 4K
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    for (uint32_t i = 0; i < num_blocks; i++) {
        if (!memory_blocks[i].used && memory_blocks[i].size >= size) {
            // Split block if too large
            if (memory_blocks[i].size > size && num_blocks < MAX_BLOCKS) {
                memory_block_t new_block;
                new_block.start_address = memory_blocks[i].start_address + size;
                new_block.size = memory_blocks[i].size - size;
                new_block.used = 0;

                memory_blocks[i].size = size;
                memory_blocks[num_blocks++] = new_block;
            }

            memory_blocks[i].used = 1;
            return (void*)memory_blocks[i].start_address;
        }
    }

    panic("Out of memory");
    return NULL;
}

void kfree(void* ptr) {
    if (!ptr) return;

    for (uint32_t i = 0; i < num_blocks; i++) {
        if (memory_blocks[i].start_address == (uint32_t)ptr) {
            memory_blocks[i].used = 0;
            
            // Merge with next block if free
            if (i < num_blocks - 1 && !memory_blocks[i + 1].used) {
                memory_blocks[i].size += memory_blocks[i + 1].size;
                for (uint32_t j = i + 1; j < num_blocks - 1; j++) {
                    memory_blocks[j] = memory_blocks[j + 1];
                }
                num_blocks--;
            }
            
            // Merge with previous block if free
            if (i > 0 && !memory_blocks[i - 1].used) {
                memory_blocks[i - 1].size += memory_blocks[i].size;
                for (uint32_t j = i; j < num_blocks - 1; j++) {
                    memory_blocks[j] = memory_blocks[j + 1];
                }
                num_blocks--;
            }
            
            return;
        }
    }
}

void panic(const char* message) {
    terminal_writestring(ERROR_STRING);
    terminal_writestring(message);
    terminal_writestring("\nSystem halted.\n");
    for(;;);
}