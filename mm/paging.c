#include "paging.h"
#include "pmm.h"
#include "idt.h"
#include "terminal.h"

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
 *
 * Additional static page tables for kernel address space.
 * PT_POOL_COUNT tables × 4 MiB each = up to 16 MiB of additional kernel
 * virtual address space can be mapped without dynamic PMM allocation for
 * page tables themselves.
 */
static uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t page_table_low[1024] __attribute__((aligned(4096)));

#define PT_POOL_COUNT 4
static uint32_t pt_pool[PT_POOL_COUNT][1024] __attribute__((aligned(4096)));
static uint32_t pt_pool_used = 0;

/* ── Internal helpers ──────────────────────────────────────────────────── */

/*
 * Allocate a zeroed page table.  Uses the static pool first; falls back
 * to PMM for frames in the identity-mapped region (< 4 MiB).
 * Returns the physical/virtual address of the new table (same value in
 * the identity-mapped early environment).
 */
static uint32_t *alloc_page_table(void)
{
    if (pt_pool_used < PT_POOL_COUNT) {
        uint32_t *pt = pt_pool[pt_pool_used++];
        /* BSS-zeroed, but zero explicitly in case of reuse */
        for (int i = 0; i < 1024; i++) pt[i] = 0;
        return pt;
    }

    /*
     * Fall back to PMM.  Request a frame below 4 MiB so it is within
     * the identity-mapped window and can be dereferenced directly.
     */
    uint32_t phys = pmm_alloc_frame_above(0);
    if (!phys) return NULL;   /* OOM – caller will panic */

    uint32_t *pt = (uint32_t *)phys;
    for (int i = 0; i < 1024; i++) pt[i] = 0;
    return pt;
}

/* Flush the TLB entry for a single virtual page. */
static inline void tlb_flush(uint32_t virt)
{
    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

/* ── Page-fault handler ────────────────────────────────────────────────── */

/*
 * Registered as the exception #14 callback in isr_handler.
 * Reads CR2 (faulting virtual address) and the CPU error code, then
 * prints a human-readable description.  isr_handler will print the full
 * register dump and call panic() after this function returns.
 */
static void page_fault_handler(registers_t *regs)
{
    uint32_t faulting_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_addr));

    uint32_t err = regs->err_code;

    terminal_writestring("  Faulting address : ");
    terminal_writehex32(faulting_addr);
    terminal_putchar('\n');

    terminal_writestring("  Reason           : ");
    terminal_writestring((err & 1u) ? "protection violation" : "page not present");
    terminal_writestring((err & 2u) ? " on WRITE"           : " on READ");
    terminal_writestring((err & 4u) ? " (user mode)"        : " (kernel mode)");
    if (err & 8u)  terminal_writestring(" [reserved-bit set in PTE]");
    if (err & 16u) terminal_writestring(" [instruction fetch]");
    terminal_putchar('\n');
}

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

    /* Register the page-fault exception handler. */
    exc_register_handler(14, page_fault_handler);
}

/* ── paging_map_page ───────────────────────────────────────────────────── */

void paging_map_page(uint32_t virt, uint32_t phys, uint32_t flags)
{
    uint32_t pde_idx = virt >> 22;
    uint32_t pte_idx = (virt >> 12) & 0x3FFu;

    uint32_t *page_table;

    if (!(page_directory[pde_idx] & PAGE_PRESENT)) {
        /* No page table exists for this 4-MiB region yet – allocate one. */
        uint32_t *pt = alloc_page_table();
        if (!pt) {
            /* Can't call panic from here directly without a header;
             * trigger a deliberate page fault instead. */
            __asm__ volatile("ud2");  /* should not reach; satisfies compiler */
            return;
        }
        page_directory[pde_idx] = (uint32_t)pt | PAGE_PRESENT | PAGE_WRITABLE;
        page_table = pt;
    } else {
        /*
         * The PDE gives us the PHYSICAL address of the page table.
         * In the identity-mapped region this equals the virtual address,
         * so the cast is safe for all early-kernel mappings.
         */
        page_table = (uint32_t *)(page_directory[pde_idx] & PAGE_FRAME_MASK);
    }

    page_table[pte_idx] = (phys & PAGE_FRAME_MASK) | (flags & 0xFFFu) | PAGE_PRESENT;
    tlb_flush(virt & PAGE_FRAME_MASK);
}

/* ── paging_unmap_page ─────────────────────────────────────────────────── */

void paging_unmap_page(uint32_t virt)
{
    uint32_t pde_idx = virt >> 22;
    uint32_t pte_idx = (virt >> 12) & 0x3FFu;

    if (!(page_directory[pde_idx] & PAGE_PRESENT))
        return;   /* no page table – nothing to unmap */

    uint32_t *page_table = (uint32_t *)(page_directory[pde_idx] & PAGE_FRAME_MASK);
    page_table[pte_idx] = 0;
    tlb_flush(virt & PAGE_FRAME_MASK);
}

/* ── paging_get_physaddr ───────────────────────────────────────────────── */

uint32_t paging_get_physaddr(uint32_t virt)
{
    uint32_t pde_idx = virt >> 22;
    uint32_t pte_idx = (virt >> 12) & 0x3FFu;

    if (!(page_directory[pde_idx] & PAGE_PRESENT))
        return 0;

    uint32_t *page_table = (uint32_t *)(page_directory[pde_idx] & PAGE_FRAME_MASK);
    uint32_t  pte        = page_table[pte_idx];

    if (!(pte & PAGE_PRESENT))
        return 0;

    /* Physical frame base + within-page offset */
    return (pte & PAGE_FRAME_MASK) | (virt & 0xFFFu);
}
