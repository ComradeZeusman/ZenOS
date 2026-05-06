#ifndef PMM_H
#define PMM_H

#include "kernel_types.h"
#include "multiboot.h"

#define PMM_PAGE_SIZE   4096u

/*
 * Maximum number of tracked physical page frames.
 * 262144 frames × 4096 bytes = 1 GiB of physical address space.
 * The bitmap occupies 262144 / 8 = 32 KiB of kernel BSS.
 */
#define PMM_MAX_FRAMES  (262144u)   /* 1 GiB */

/*
 * Initialise the PMM from the GRUB multiboot information structure.
 * Must be called once, before any pmm_alloc_frame() call.
 * Marks all memory as reserved, then frees every page reported as
 * MULTIBOOT_MEMORY_AVAILABLE, and re-reserves the low 1 MB and the
 * kernel image (0x100000 … _kernel_end).
 */
void     pmm_init(multiboot_info_t *mbi);

/*
 * Allocate one 4 KiB physical page frame.
 * Returns the physical base address of the frame, or 0 on OOM.
 */
uint32_t pmm_alloc_frame(void);

/*
 * Release a previously allocated frame back to the free pool.
 * addr must be the value returned by pmm_alloc_frame(); passing any
 * other address is undefined behaviour.
 */
void     pmm_free_frame(uint32_t addr);

/*
 * Allocate one 4 KiB physical page frame whose physical address is >= min_addr.
 * Useful when the caller needs a frame that is guaranteed to be within the
 * identity-mapped window (pass 0 for no constraint).
 * Returns the physical base address, or 0 on OOM.
 */
uint32_t pmm_alloc_frame_above(uint32_t min_addr);

/*
 * Mark every page frame that overlaps [base, base+length) as reserved.
 * Safe to call multiple times; re-reserving an already-reserved frame
 * is a no-op.  Length is rounded up to a page boundary.
 */
void pmm_reserve_region(uint32_t base, uint32_t length);

/*
 * Mark every page frame that overlaps [base, base+length) as free.
 * Only whole page frames whose base is within [base, base+length) are
 * touched.  Freeing an already-free frame is a no-op.
 */
void pmm_free_region(uint32_t base, uint32_t length);

/* Query helpers */
uint32_t pmm_get_free_frames(void);   /* number of free 4 KiB frames  */
uint32_t pmm_get_total_frames(void);  /* total usable frames seen      */

#endif /* PMM_H */
