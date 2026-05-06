#ifndef HEAP_H
#define HEAP_H

#include "kernel_types.h"

/*
 * heap_init()
 *
 * Must be called once, after paging_init() and pmm_init().
 * Sets up the initial kernel heap immediately above the kernel image
 * (starting at _kernel_end rounded up to a page boundary) and commits
 * the first HEAP_INITIAL_PAGES pages of physical memory to it.
 */
void heap_init(void);

/*
 * kmalloc() – allocate at least `size` bytes from the kernel heap.
 *
 * Returned pointer is always 8-byte aligned.
 * Returns NULL if the heap is exhausted (should be treated as fatal).
 */
void *kmalloc(size_t size);

/*
 * kfree() – return a previously kmalloc'd block to the heap.
 *
 * Passing NULL is a no-op.
 * Passing a pointer that was not returned by kmalloc() is undefined
 * behaviour (the magic check will catch obvious mistakes and panic).
 */
void kfree(void *ptr);

#endif /* HEAP_H */
