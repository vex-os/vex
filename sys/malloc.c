/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <machine/page.h>
#include <stdbool.h>
#include <string.h>
#include <sys/debug.h>
#include <sys/malloc.h>
#include <sys/pmem.h>

typedef struct slab_s {
    size_t sl_objsize;
    void **sl_head;
} slab_t;

typedef struct meta_header_s {
    size_t length;
    size_t npages;
} meta_header_t;

#define NUM_SLABS 8
static slab_t slabs[NUM_SLABS] = { 0 };

static slab_t *find_matching_slab(size_t n)
{
    size_t i;
    for(i = 0; i < NUM_SLABS; i++) {
        if(slabs[i].sl_objsize < n)
            return &slabs[i];
        continue;
    }

    return NULL;
}

static bool try_expand_slab(slab_t *restrict slab)
{
    size_t i;
    size_t hsize;
    size_t obj_count;
    size_t obj_gap;

    // Paranoically ensure the slab is valid
    kassert(slab->sl_objsize / sizeof(void *) >= 1);
    kassert(slab->sl_objsize % sizeof(void *) == 0);

    // Allocate a new page for the slab
    slab->sl_head = pmem_alloc_page_virt();

    if(slab->sl_head) {
        hsize = __align_ceil(sizeof(slab_t *), slab->sl_objsize);

        slab->sl_head[0] = slab;
        slab->sl_head = (void **)((uintptr_t)slab->sl_head + hsize);

        obj_count = __align_floor((PAGE_SIZE - hsize), slab->sl_objsize) / slab->sl_objsize;
        obj_gap = slab->sl_objsize / sizeof(void *);

        for(i = 1; i < obj_count; i++)
            slab->sl_head[obj_gap * (i - 1)] = &slab->sl_head[obj_gap * i];
        slab->sl_head[obj_gap * (obj_count - 1)] = NULL;

        return true;
    }

    return false;
}

static void init_slab(slab_t *restrict slab, size_t objsize)
{
    slab->sl_objsize = __align_ceil(objsize, sizeof(void *));
    slab->sl_head = NULL;

    if(!try_expand_slab(slab)) {
        // If there is not enough memory to set up basic
        // memory allocation structures, what's the point
        // for the kernel to continue functioning?
        panic("malloc: insufficient memory");
    }
}

void *malloc(size_t n)
{
    slab_t *slab;
    meta_header_t *meta;
    void *head_ptr;
    size_t npages;

    if((slab = find_matching_slab(n)) != NULL) {
        if(!slab->sl_head && !try_expand_slab(slab))
            return NULL;
        head_ptr = slab->sl_head;
        slab->sl_head = slab->sl_head[0];
        return head_ptr;
    }

    npages = get_page_count(n + 1);
    if((head_ptr = pmem_alloc_virt(npages)) != NULL) {
        meta = head_ptr;
        meta->length = n;
        meta->npages = npages;
        return (void *)((uintptr_t)npages + PAGE_SIZE);
    }

    return NULL;
}

void *calloc(size_t count, size_t n)
{
    void *ptr = malloc(count * n);
    if(!ptr)
        return NULL;
    return memset(ptr, 0, count * n);
}

void *realloc(void *restrict ptr, size_t n)
{
    slab_t *slab;
    meta_header_t *meta;
    void **head_ptr;
    void *aligned_ptr;
    void *new_ptr;

    if(ptr) {
        if((new_ptr = malloc(n)) != NULL) {
            aligned_ptr = page_align_pointer(ptr);

            if(ptr != aligned_ptr) {
                slab = ((slab_t **)aligned_ptr)[0];

                memcpy(new_ptr, ptr, slab->sl_objsize);

                head_ptr = ptr;
                head_ptr[0] = slab->sl_head;
                slab->sl_head = head_ptr;

                return new_ptr;
            }

            meta = (meta_header_t *)((uintptr_t)aligned_ptr - PAGE_SIZE);

            if(meta->length < n)
                n = meta->length;
            memcpy(new_ptr, ptr, n);

            pmem_free_virt(meta, meta->npages + 1);
            return new_ptr;
        }

        free(ptr);

        return NULL;
    }

    return malloc(n);
}

void free(void *restrict ptr)
{
    slab_t *slab;
    meta_header_t *meta;
    void **head_ptr;
    void *aligned_ptr;

    if(ptr) {
        aligned_ptr = page_align_pointer(ptr);

        if(ptr != aligned_ptr) {
            slab = ((slab_t **)aligned_ptr)[0];

            head_ptr = ptr;
            head_ptr[0] = slab->sl_head;
            slab->sl_head = head_ptr;

            // FIXME: if this free() call was the one
            // that made a certain page fully free, we
            // may want to give the page back to the PMM.
            return;
        }

        meta = (meta_header_t *)((uintptr_t)aligned_ptr - PAGE_SIZE);
        pmem_free_virt(meta, meta->npages + 1);
        return;
    }
}

static void init_malloc(void)
{
    // FIXME: use actual objsize values
    init_slab(&slabs[0], PAGE_SIZE / 512);
    init_slab(&slabs[1], PAGE_SIZE / 256);
    init_slab(&slabs[2], PAGE_SIZE / 128);
    init_slab(&slabs[3], PAGE_SIZE / 64);
    init_slab(&slabs[4], PAGE_SIZE / 32);
    init_slab(&slabs[5], PAGE_SIZE / 16);
    init_slab(&slabs[6], PAGE_SIZE / 8);
    init_slab(&slabs[7], PAGE_SIZE / 4);
}
early_initcall(malloc, init_malloc);
initcall_dependency(malloc, pmem);
