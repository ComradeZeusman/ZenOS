#include "pmm.h"
#include "multiboot.h"

/* Linker-exported symbol – end of the kernel image (BSS included) */
extern uint32_t _kernel_end;

/* ── Bitmap ────────────────────────────────────────────────────────────
 *
 * Each bit represents one 4 KiB physical page frame:
 *   1 = used / reserved
 *   0 = free
 *
 * Stored in BSS so it is zero-initialised by the loader (all free),
 * but pmm_init() explicitly marks everything used before freeing only
 * the regions reported as available by the bootloader.
 *
 * Layout:  pmm_bitmap[i]  bit b  →  frame (i*32 + b)
 *          physical address = frame * PMM_PAGE_SIZE
 */
static uint32_t pmm_bitmap[PMM_MAX_FRAMES / 32];

static uint32_t pmm_total_frames  = 0;
static uint32_t pmm_used_frames   = 0;
/*
 * Allocation hint: index of the bitmap word where the last successful
 * alloc found a free bit.  Avoids rescanning the fully-used low frames
 * on every allocation, giving O(1) amortised performance.
 */
static uint32_t pmm_first_free_word = 0;

/* ── Internal bit helpers ──────────────────────────────────────────── */

static inline void bitmap_set(uint32_t frame)
{
    pmm_bitmap[frame >> 5] |= (1u << (frame & 31u));
}

static inline void bitmap_clear(uint32_t frame)
{
    pmm_bitmap[frame >> 5] &= ~(1u << (frame & 31u));
}

static inline int bitmap_test(uint32_t frame)
{
    return !!(pmm_bitmap[frame >> 5] & (1u << (frame & 31u)));
}

/* ── Region helpers (public) ───────────────────────────────────────── */

void pmm_free_region(uint32_t base, uint32_t length)
{
    /* Align base up to the next page boundary */
    uint32_t align = base & (PMM_PAGE_SIZE - 1u);
    if (align) {
        uint32_t pad = PMM_PAGE_SIZE - align;
        if (pad >= length) return;
        base   += pad;
        length -= pad;
    }

    uint32_t frame = base / PMM_PAGE_SIZE;
    uint32_t count = length / PMM_PAGE_SIZE;

    for (uint32_t i = 0; i < count; i++, frame++) {
        if (frame >= PMM_MAX_FRAMES) break;
        if (bitmap_test(frame)) {
            bitmap_clear(frame);
            pmm_used_frames--;
            if ((frame >> 5) < pmm_first_free_word)
                pmm_first_free_word = frame >> 5;
        }
    }
}

void pmm_reserve_region(uint32_t base, uint32_t length)
{
    uint32_t frame = base / PMM_PAGE_SIZE;
    uint32_t count = (length + PMM_PAGE_SIZE - 1u) / PMM_PAGE_SIZE;

    for (uint32_t i = 0; i < count; i++, frame++) {
        if (frame >= PMM_MAX_FRAMES) break;
        if (!bitmap_test(frame)) {
            bitmap_set(frame);
            pmm_used_frames++;
        }
    }
}

/* ── Public API ─────────────────────────────────────────────────────── */

void pmm_init(multiboot_info_t *mbi)
{
    /* Step 1 – mark every frame as used (all bits = 1). */
    for (uint32_t i = 0; i < PMM_MAX_FRAMES / 32u; i++)
        pmm_bitmap[i] = 0xFFFFFFFFu;
    pmm_used_frames  = PMM_MAX_FRAMES;
    pmm_total_frames = 0;

    /* Step 2 – walk the bootloader memory map and free available regions. */
    if (mbi->flags & MULTIBOOT_INFO_MMAP) {
        multiboot_mmap_entry_t *entry =
            (multiboot_mmap_entry_t *)(mbi->mmap_addr);
        multiboot_mmap_entry_t *map_end =
            (multiboot_mmap_entry_t *)(mbi->mmap_addr + mbi->mmap_length);

        /* First pass: determine total usable frame count. */
        multiboot_mmap_entry_t *e = entry;
        while (e < map_end) {
            if (e->type == MULTIBOOT_MEMORY_AVAILABLE) {
                uint32_t last   = e->base_low + e->length_low;
                uint32_t frames = last / PMM_PAGE_SIZE;
                if (frames > pmm_total_frames)
                    pmm_total_frames = frames;
            }
            e = (multiboot_mmap_entry_t *)
                ((uint32_t)e + e->size + (uint32_t)sizeof(e->size));
        }
        if (pmm_total_frames > PMM_MAX_FRAMES)
            pmm_total_frames = PMM_MAX_FRAMES;

        /* Second pass: free all AVAILABLE regions. */
        while (entry < map_end) {
            if (entry->type == MULTIBOOT_MEMORY_AVAILABLE)
                pmm_free_region(entry->base_low, entry->length_low);
            entry = (multiboot_mmap_entry_t *)
                    ((uint32_t)entry + entry->size + (uint32_t)sizeof(entry->size));
        }

    } else if (mbi->flags & MULTIBOOT_INFO_MEM) {
        /* Fallback: mem_upper kilobytes of RAM above 1 MB. */
        uint32_t upper_bytes = (uint32_t)mbi->mem_upper * 1024u;
        pmm_total_frames = (0x100000u + upper_bytes) / PMM_PAGE_SIZE;
        if (pmm_total_frames > PMM_MAX_FRAMES)
            pmm_total_frames = PMM_MAX_FRAMES;
        pmm_free_region(0x100000u, upper_bytes);

    } else {
        /* Last resort: assume 16 MiB. */
        pmm_total_frames = 0x1000000u / PMM_PAGE_SIZE;
        pmm_free_region(0x100000u, 0x1000000u - 0x100000u);
    }

    /*
     * Step 3 – unconditionally re-reserve regions that must never be
     * handed to callers, regardless of what the memory map says.
     *
     * [0x000000 – 0x0FFFFF]  Real-mode IVT, BDA, EBDA, VGA buffer, BIOS ROM
     * [0x100000 – _kernel_end]  Kernel image + BSS (includes this bitmap)
     */
    pmm_reserve_region(0x0u, 0x100000u);

    uint32_t kend = (uint32_t)&_kernel_end;
    if (kend > 0x100000u)
        pmm_reserve_region(0x100000u, kend - 0x100000u);
}

uint32_t pmm_alloc_frame(void)
{
    return pmm_alloc_frame_above(0);
}

uint32_t pmm_alloc_frame_above(uint32_t min_addr)
{
    uint32_t start_word  = min_addr / PMM_PAGE_SIZE / 32u;
    uint32_t words       = (pmm_total_frames + 31u) / 32u;

    /* Start scanning from whichever is higher: hint or min_addr word. */
    uint32_t scan_start = (pmm_first_free_word > start_word)
                          ? pmm_first_free_word : start_word;

    for (uint32_t i = scan_start; i < words; i++) {
        if (pmm_bitmap[i] == 0xFFFFFFFFu)
            continue;

        for (uint32_t bit = 0; bit < 32u; bit++) {
            uint32_t frame = i * 32u + bit;
            if (frame >= pmm_total_frames) return 0;
            uint32_t phys = frame * PMM_PAGE_SIZE;
            if (phys < min_addr) continue;
            if (!bitmap_test(frame)) {
                bitmap_set(frame);
                pmm_used_frames++;
                pmm_first_free_word = i;   /* update hint */
                return phys;
            }
        }
    }
    return 0;   /* out of memory */
}

void pmm_free_frame(uint32_t addr)
{
    uint32_t frame = addr / PMM_PAGE_SIZE;
    if (frame >= PMM_MAX_FRAMES) return;
    if (bitmap_test(frame)) {
        bitmap_clear(frame);
        pmm_used_frames--;
        /* Retreat hint so this frame becomes available again immediately. */
        if ((frame >> 5) < pmm_first_free_word)
            pmm_first_free_word = frame >> 5;
    }
}

uint32_t pmm_get_free_frames(void)
{
    return pmm_total_frames - pmm_used_frames;
}

uint32_t pmm_get_total_frames(void)
{
    return pmm_total_frames;
}
