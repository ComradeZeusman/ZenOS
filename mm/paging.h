#ifndef PAGING_H
#define PAGING_H

#include "kernel_types.h"

/* ── Page-table entry flag bits ──────────────────────────────────────── */
#define PAGE_PRESENT    (1u << 0)   /* entry is valid                    */
#define PAGE_WRITABLE   (1u << 1)   /* read/write (clear = read-only)    */
#define PAGE_USER       (1u << 2)   /* user-mode accessible              */
#define PAGE_ACCESSED   (1u << 5)   /* set by CPU on first access        */
#define PAGE_DIRTY      (1u << 6)   /* set by CPU on first write         */

/* ── Physical address mask ───────────────────────────────────────────── */
/* Bits [31:12] of a PDE/PTE hold the physical page-frame number.         */
#define PAGE_FRAME_MASK (~0xFFFu)

/*
 * paging_init()
 *
 * Identity-maps the first 4 MiB of physical memory (covering the kernel
 * image, PMM bitmap, VGA buffer at 0xB8000, and all other early
 * structures), then enables paging by:
 *   1. Writing the page-directory physical address into CR3.
 *   2. Setting bit 31 (PG) of CR0.
 *
 * Must be called after pmm_init() but before any code that relies on
 * virtual-address translation.
 */
void paging_init(void);

#endif /* PAGING_H */
