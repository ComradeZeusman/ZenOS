#include "heap.h"
#include "pmm.h"
#include "paging.h"
#include "terminal.h"

/* ── Configuration ─────────────────────────────────────────────────────── */

/*
 * Upper ceiling of the heap's virtual address range.
 * Must remain within the first 4 MiB (identity-mapped by paging_init).
 */
#define HEAP_MAX_VIRT   0x00400000u   /* 4 MiB */

/*
 * Number of pages committed to the heap on each expansion call.
 * 4 pages = 16 KiB per expansion step.
 */
#define HEAP_EXPAND_PAGES 4u

/*
 * Minimum usable size for a block to be split.  Splitting smaller than
 * this wastes more memory on headers than it saves.
 */
#define HEAP_MIN_SPLIT  (sizeof(heap_block_t) + 8u)

/* ── Block header ──────────────────────────────────────────────────────── */

/*
 * Every allocation is preceded by a heap_block_t header.
 * Blocks are kept in a singly-linked list ordered by address.
 *
 * sizeof(heap_block_t) == 16 bytes on i686 (4+4+4+4).
 * User data begins immediately after the header, so with a page-aligned
 * heap_base, all user pointers are naturally 16-byte aligned.
 */
#define HEAP_MAGIC_FREE 0xFEEDF00Du
#define HEAP_MAGIC_USED 0xC0DEC0DEu

typedef struct heap_block {
    uint32_t           magic;   /* HEAP_MAGIC_FREE or HEAP_MAGIC_USED    */
    uint32_t           size;    /* usable bytes (NOT counting header)    */
    uint32_t           used;    /* 0 = free, 1 = allocated               */
    struct heap_block *next;    /* next block in address order, or NULL  */
} heap_block_t;

/* ── Heap state ────────────────────────────────────────────────────────── */

static heap_block_t *heap_first = NULL;  /* first block (lowest address)  */
static uint32_t      heap_limit = 0;     /* first uncommitted virtual byte */
static uint32_t      heap_base  = 0;     /* base virtual address of heap   */

/* ── Internal helpers ──────────────────────────────────────────────────── */

extern uint32_t _kernel_end;   /* linker-exported end of kernel image */
extern void panic(const char *);

/*
 * Round `addr` up to the next multiple of `align` (must be a power of 2).
 */
static inline uint32_t align_up(uint32_t addr, uint32_t align)
{
    return (addr + align - 1u) & ~(align - 1u);
}

/*
 * heap_expand() – commit HEAP_EXPAND_PAGES more pages to the heap.
 *
 * In the identity-mapped region, virtual address == physical address, so
 * we just tell the PMM to mark those frames as used (they are already
 * mapped by page_table_low).
 *
 * Returns a pointer to a free heap_block_t covering the new region, or
 * NULL if there is no space left below HEAP_MAX_VIRT.
 */
static heap_block_t *heap_expand(void)
{
    uint32_t expand_bytes = HEAP_EXPAND_PAGES * PMM_PAGE_SIZE;

    if (heap_limit + expand_bytes > HEAP_MAX_VIRT)
        return NULL;   /* heap ceiling reached */

    /* Mark the physical frames as reserved (identity-mapped: virt==phys). */
    pmm_reserve_region(heap_limit, expand_bytes);

    /* Create one large free block covering the new region. */
    heap_block_t *blk = (heap_block_t *)heap_limit;
    blk->magic = HEAP_MAGIC_FREE;
    blk->size  = expand_bytes - sizeof(heap_block_t);
    blk->used  = 0;
    blk->next  = NULL;

    heap_limit += expand_bytes;
    return blk;
}

/*
 * heap_append() – attach `blk` to the end of the block list, or coalesce
 * it with the current tail if the tail is also free.
 */
static void heap_append(heap_block_t *blk)
{
    if (!heap_first) {
        heap_first = blk;
        return;
    }

    /* Walk to the last block */
    heap_block_t *tail = heap_first;
    while (tail->next) tail = tail->next;

    /* Coalesce with tail if it is free and physically adjacent */
    heap_block_t *expected_next =
        (heap_block_t *)((uint8_t *)tail + sizeof(heap_block_t) + tail->size);

    if (!tail->used && expected_next == blk) {
        tail->size += sizeof(heap_block_t) + blk->size;
        /* tail->next stays NULL (blk had next==NULL) */
    } else {
        tail->next = blk;
    }
}

/* ── Public API ────────────────────────────────────────────────────────── */

void heap_init(void)
{
    /* Start the heap right after the kernel image, page-aligned. */
    heap_base  = align_up((uint32_t)&_kernel_end, PMM_PAGE_SIZE);
    heap_limit = heap_base;

    /* Commit the first batch of pages and build the initial free block. */
    heap_block_t *initial = heap_expand();
    if (!initial)
        panic("heap_init: no space above kernel image");

    heap_first = initial;
}

void *kmalloc(size_t size)
{
    if (size == 0) return NULL;

    /* Round up to 8-byte alignment so all returned pointers are aligned. */
    size = align_up((uint32_t)size, 8u);

    /* First-fit scan */
    heap_block_t *blk = heap_first;
    while (blk) {
        if (!blk->used && blk->size >= size) {
            /*
             * Try to split the block if the leftover is large enough to
             * form a useful free block on its own.
             */
            if (blk->size >= size + HEAP_MIN_SPLIT) {
                heap_block_t *split =
                    (heap_block_t *)((uint8_t *)blk + sizeof(heap_block_t) + size);
                split->magic = HEAP_MAGIC_FREE;
                split->size  = blk->size - size - sizeof(heap_block_t);
                split->used  = 0;
                split->next  = blk->next;

                blk->next = split;
                blk->size = size;
            }

            blk->magic = HEAP_MAGIC_USED;
            blk->used  = 1;
            return (void *)((uint8_t *)blk + sizeof(heap_block_t));
        }
        blk = blk->next;
    }

    /* No suitable block found – expand the heap and retry once. */
    heap_block_t *new_blk = heap_expand();
    if (!new_blk) {
        panic("kmalloc: kernel heap exhausted");
        return NULL;
    }
    heap_append(new_blk);
    return kmalloc(size);   /* tail-recurse: guaranteed to find space now */
}

void kfree(void *ptr)
{
    if (!ptr) return;

    heap_block_t *blk = (heap_block_t *)((uint8_t *)ptr - sizeof(heap_block_t));

    /* Validate magic – catch double-free or wild pointer */
    if (blk->magic != HEAP_MAGIC_USED) {
        if (blk->magic == HEAP_MAGIC_FREE)
            panic("kfree: double-free detected");
        else
            panic("kfree: invalid pointer (heap corruption)");
        return;
    }

    blk->magic = HEAP_MAGIC_FREE;
    blk->used  = 0;

    /*
     * Forward coalescing: merge with the immediately following block if
     * it is also free and physically adjacent.
     */
    while (blk->next && !blk->next->used) {
        heap_block_t *next = blk->next;
        /* Verify adjacency (detects corruption) */
        heap_block_t *expected =
            (heap_block_t *)((uint8_t *)blk + sizeof(heap_block_t) + blk->size);
        if (expected != next) break;   /* gap – don't merge */

        blk->size += sizeof(heap_block_t) + next->size;
        blk->next  = next->next;
    }
}
