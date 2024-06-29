// SPDX-License-Identifier: Zlib
#include <kern/assert.h>
#include <kern/panic.h>
#include <kern/printf.h>
#include <mm/page.h>
#include <mm/pmm.h>
#include <mm/slab.h>
#include <string.h>

struct slab {
    size_t sl_blocksize;
    void **sl_head;
};

static size_t num_slab = 0;
static struct slab *slabs = NULL;

static __always_inline __nodiscard inline size_t calc_blocksize(size_t index)
{
    return ((1 << index) * sizeof(void *));
}

static struct slab *find_slab(size_t sz)
{
    size_t i;

    // Try to find exact size match
    for(i = 0; slabs[i].sl_blocksize; ++i) {
        if(slabs[i].sl_blocksize == sz)
            return &slabs[i];
        continue;
    }

    // Give up and find anything that fits
    for(i = 0; slabs[i].sl_blocksize; ++i) {
        if(slabs[i].sl_blocksize >= sz)
            return &slabs[i];
        continue;
    }

    return NULL;
}

static int expand_slab(struct slab *restrict sl)
{
    size_t i;
    size_t header;
    size_t count, gap;

    kassert((sl->sl_blocksize / sizeof(void *)) >= 1);
    kassert((sl->sl_blocksize % sizeof(void *)) == 0);

    if((sl->sl_head = pmm_alloc_hhdm()) != NULL) {
        header = align_ceil(sizeof(struct slab *), sl->sl_blocksize);

        sl->sl_head[0] = sl;
        sl->sl_head = (void **)((uintptr_t)sl->sl_head + header);

        // Each page contains a chunk of the linked list
        // prepended with a header that takes at least a single
        // block of sl_blocksize bytes off the available space
        count = align_floor((PAGE_SIZE - header), sl->sl_blocksize) / sl->sl_blocksize;
        gap = sl->sl_blocksize / sizeof(void *);

        for(i = 1; i < count; ++i)
            sl->sl_head[gap * (i - 1)] = &sl->sl_head[gap * i];
        sl->sl_head[gap * (count - 1)] = NULL;

        return 1;
    }

    return 0;
}

void *slab_alloc(size_t sz)
{
    struct slab *sl;
    void **headptr;

    if((sl = find_slab(sz)) != NULL) {
        if(!sl->sl_head && !expand_slab(sl))
            return NULL;
        
        headptr = sl->sl_head;
        sl->sl_head = headptr[0];
        return headptr;
    }

    return NULL;
}

void *slab_calloc(size_t count, size_t sz)
{
    void *ptr = slab_alloc(count * sz);
    if(ptr == NULL)
        return NULL;
    return memset(ptr, 0, count * sz);
}

void *slab_realloc(void *restrict ptr, size_t sz)
{
    struct slab *sl;
    void **headptr;
    void *newptr;

    if(ptr != NULL) {
        if((newptr = slab_alloc(sz)) != NULL) {
            sl = ((struct slab **)(page_align_ptr(ptr)))[0];

            memcpy(newptr, ptr, sl->sl_blocksize);

            headptr = ptr;
            headptr[0] = sl->sl_head;
            sl->sl_head = headptr;

            return newptr;
        }

        slab_free(ptr);

        return NULL;
    }

    return slab_alloc(sz);
}

void slab_free(void *restrict ptr)
{
    struct slab *sl;
    void **headptr;

    if(ptr != NULL) {
        sl = ((struct slab **)(page_align_ptr(ptr)))[0];

        headptr = ptr;
        headptr[0] = sl->sl_head;
        sl->sl_head = headptr;

        return;
    }
}

void init_slab(void)
{
    size_t i;
    size_t npages;

    for(num_slab = 0;; num_slab += 1) {
        if(calc_blocksize(num_slab) <= (PAGE_SIZE / 4))
            continue;
        break;
    }

    npages = page_count(num_slab * sizeof(struct slab));
    slabs = dma_alloc_hhdm(npages);

    if(slabs == NULL) {
        panic("slab: out of memory (%zu)", npages);
        unreachable();
    }

    for(i = 0; i < num_slab; ++i) {
        slabs[i].sl_blocksize = calc_blocksize(i);
        slabs[i].sl_head = NULL;

        kprintf(KP_DEBUG, "slab: slabs[%zu].sl_blocksize=%zu", i, slabs[i].sl_blocksize);

        if(!expand_slab(&slabs[i])) {
            panic("slab: out of memory");
            unreachable();
        }
    }
}
