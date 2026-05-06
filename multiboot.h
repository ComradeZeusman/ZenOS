#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include "kernel_types.h"

/* Value GRUB places in EAX before jumping to our entry point */
#define MULTIBOOT_BOOTLOADER_MAGIC  0x2BADB002

/* Bits in the multiboot header we embed in the binary */
#define MULTIBOOT_HEADER_ALIGN      (1 << 0)   /* align modules on page boundaries   */
#define MULTIBOOT_HEADER_MEMINFO    (1 << 1)   /* request memory map from bootloader */

/* Bits in multiboot_info_t.flags – tell us which fields are valid */
#define MULTIBOOT_INFO_MEM          (1 << 0)   /* mem_lower / mem_upper valid         */
#define MULTIBOOT_INFO_MMAP         (1 << 6)   /* mmap_addr / mmap_length valid       */

/* Memory region types used in mmap entries */
#define MULTIBOOT_MEMORY_AVAILABLE  1
#define MULTIBOOT_MEMORY_RESERVED   2

/*
 * One entry in the BIOS memory map provided by GRUB.
 *
 * The 'size' field holds the byte length of the remaining fields
 * (i.e. NOT including 'size' itself).  The next entry is at:
 *   (uint8_t *)entry + entry->size + sizeof(entry->size)
 *
 * A standard entry has size = 20.
 */
typedef struct {
    uint32_t size;
    uint32_t base_low;     /* bits  0-31 of the base address  */
    uint32_t base_high;    /* bits 32-63 of the base address  */
    uint32_t length_low;   /* bits  0-31 of the region length */
    uint32_t length_high;  /* bits 32-63 of the region length */
    uint32_t type;         /* MULTIBOOT_MEMORY_* constant     */
} __attribute__((packed)) multiboot_mmap_entry_t;

/*
 * The Multiboot information structure – GRUB fills this and passes
 * a pointer to it in EBX before calling our entry point.
 *
 * Only the fields we actually use are fully listed; the struct is
 * padded to the correct offsets so sizeof() and pointer arithmetic
 * remain correct regardless of which fields are read.
 */
typedef struct {
    uint32_t flags;          /* which fields below are valid            */
    uint32_t mem_lower;      /* KB of lower memory  (< 1 MB)           */
    uint32_t mem_upper;      /* KB of upper memory  (> 1 MB)           */
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint8_t  syms[16];       /* ELF / a.out symbol table – 16 bytes    */
    uint32_t mmap_length;    /* byte length of the memory-map array    */
    uint32_t mmap_addr;      /* physical address of first mmap entry   */
} __attribute__((packed)) multiboot_info_t;

#endif /* MULTIBOOT_H */
