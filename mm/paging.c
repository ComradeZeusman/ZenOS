#include "paging.h"

/*
 * ── Static page structures ──────────────────────────────────────────────
 *
 * Both arrays live in BSS (zero-initialised by the loader / boot stub),
 * and are forced to a 4 KiB boundary so they can be placed directly into
 * CR3 / a PDE without masking.
 *
 * page_directory  – the single Page Directory (PD) used by the kernel.
 * page_table_low  – the Page Table that identity-maps the first 4 MiB
 *                   (PDE index 0, virtual 0x00000000–0x003FFFFF).
 *
 * 4 MiB covers:
 *   0x00000000 – 0x000FFFFF  real-mode / BDA / EBDA / VGA / BIOS ROM
 *   0x00100000 – 0x003FFFFF  kernel text, rodata, data, BSS (PMM bitmap)
 */
static uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t page_table_low[1024] __attribute__((aligned(4096)));

/* ── paging_init ───────────────────────────────────────────────────────── */

void paging_init(void)
{
    /*
     * Step 1 – build the identity-map page table.
     *
     * page_table_low[i] maps virtual address (i × 4096) to the same
     * physical address.  All 1024 entries are marked Present + Writable,
     * covering the full first 4 MiB.
     */
    for (uint32_t i = 0; i < 1024u; i++) {
        page_table_low[i] = (i * 0x1000u) | PAGE_PRESENT | PAGE_WRITABLE;
    }

    /*
     * Step 2 – install PDE[0] pointing at page_table_low.
     *
     * All other PDEs remain 0 (not-present); any access outside the first
     * 4 MiB will page-fault, which is the correct early-boot behaviour.
     */
    page_directory[0] = (uint32_t)page_table_low | PAGE_PRESENT | PAGE_WRITABLE;

    /*
     * Step 3 – load CR3 with the physical address of the page directory.
     *
     * Because we are still executing with flat (identity) physical
     * addressing before paging is enabled, &page_directory IS the
     * physical address.
     */
    __asm__ volatile("mov %0, %%cr3" :: "r"(page_directory) : "memory");

    /*
     * Step 4 – set CR0.PG (bit 31) to enable paging.
     *
     * After this instruction the CPU translates every memory access
     * through the page directory just loaded.  The next fetch is at
     * the instruction following this block; because it is identity-mapped
     * (virtual == physical) execution continues without a fault.
     */
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0) : "memory");
}
