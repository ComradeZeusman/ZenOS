#include "kernel_types.h"
#include "gdt.h"
#include "idt.h"

#define MEMORY_END 0x1000000  // 16MB of memory
#define PAGE_SIZE 4096
#define ERROR_STRING "\033[31m[ERROR]\033[0m "

// Function prototypes
void terminal_initialize(void);
void terminal_writestring(const char* str);
void terminal_putchar(char c);
void init_memory(void);
void panic(const char* message);
void* kmalloc(size_t size);
void kfree(void* ptr);

// Ensure kernel_main is not removed by the linker
void kernel_main(void) __attribute__((used));

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

// Kernel entry point
void kernel_main() {
    // Disable interrupts for the entire hardware-init sequence.
    // The bootloader already executed CLI before the protected-mode switch,
    // but we assert it here explicitly so the invariant is self-documenting.
    __asm__ volatile("cli");

    // Initialize terminal for output
    terminal_initialize();

    // Print welcome message
    terminal_writestring("ZenOS Kernel Initializing...\n");

    // Re-install the GDT from kernel space, replacing the bootloader's
    // temporary copy that lives inside the now-reused first 512 bytes.
    gdt_install();
    terminal_writestring("GDT installed\n");

    // Initialize memory management
    init_memory();
    terminal_writestring("Memory management initialized\n");

    // Set up the Interrupt Descriptor Table and load IDTR
    idt_install();
    terminal_writestring("IDT installed\n");

    // Hardware init complete – re-enable interrupts.
    __asm__ volatile("sti");
    
    // Prevent kernel from returning
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

// Memory management implementation
void init_memory() {
    // Initialize page directory
    page_directory_t *kernel_directory = (page_directory_t*)0x9C000;
    for(int i = 0; i < 1024; i++) {
        kernel_directory->tables[i] = 0;
        kernel_directory->physical_tables[i] = 0;
    }

    // Initialize physical memory manager
    memory_blocks[0].start_address = 0x100000; // Start after 1MB
    memory_blocks[0].size = MEMORY_END - 0x100000;
    memory_blocks[0].used = 0;
    num_blocks = 1;
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