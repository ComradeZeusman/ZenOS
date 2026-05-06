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
 * Also registers the page-fault exception handler (#PF, vector 14).
 *
 * Must be called after pmm_init() but before any code that relies on
 * virtual-address translation.
 */
void paging_init(void);

/*
 * paging_map_page() – create or update a virtual→physical mapping.
 *
 * virt  : virtual address (will be truncated to page boundary)
 * phys  : physical address (will be truncated to page boundary)
 * flags : combination of PAGE_PRESENT / PAGE_WRITABLE / PAGE_USER etc.
 *         PAGE_PRESENT is OR'd in automatically.
 *
 * If no page table exists for the given PDE, one is allocated from the
 * PMM (must be within the identity-mapped region so it can be zeroed).
 * Flushes the TLB entry for virt via INVLPG.
 */
void paging_map_page(uint32_t virt, uint32_t phys, uint32_t flags);

/*
 * paging_unmap_page() – remove a virtual→physical mapping.
 *
 * Clears the PTE for virt and flushes the TLB.  Does nothing if the
 * page is not currently mapped.
 */
void paging_unmap_page(uint32_t virt);

/*
 * paging_get_physaddr() – translate a virtual address to physical.
 *
 * Returns the physical address that virt currently maps to, including
 * the within-page offset.  Returns 0 if the page is not present.
 */
uint32_t paging_get_physaddr(uint32_t virt);

#endif /* PAGING_H */
