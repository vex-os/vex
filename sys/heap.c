/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <stdbool.h>
#include <string.h>
#include <sys/heap.h>
#include <sys/page.h>
#include <sys/pmem.h>
#include <sys/systm.h>

typedef struct meta_s {
    size_t length;
    size_t npages;
} meta_t;

typedef struct slab_s {
    size_t blocksize;
    void **head;
} slab_t;

static slab_t *slabs = NULL;

static slab_t *find_matching_slab(size_t n)
{
    slab_t *slab;

    for(slab = slabs; slab; ++slab) {
        if(slab->blocksize <= n)
            return slab;
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

    kassert(slab->blocksize / sizeof(void *) >= 1);
    kassert(slab->blocksize % sizeof(void *) == 0);

    if((slab->head = pmem_alloc_highmem(1)) != NULL) {
        hsize = __align_ceil(sizeof(slab_t *), slab->blocksize);

        slab->head[0] = slab;
        slab->head = (void **)((uintptr_t)slab->head + hsize);

        /* Linked list will have obj_count new entries,
         * each of them pointing to the next one obj_gap bytes forward */
        obj_count = __align_floor((PAGE_SIZE - hsize), slab->blocksize) / slab->blocksize;
        obj_gap = slab->blocksize / sizeof(void *);

        for(i = 1; i < obj_count; i++)
            slab->head[obj_gap * (i - 1)] = &slab->head[obj_gap * i];
        slab->head[obj_gap * (obj_count - 1)] = NULL;

        return true;
    }

    return false;
}

static void init_slab(slab_t *restrict slab, size_t blocksize)
{
    slab->blocksize = __align_ceil(blocksize, sizeof(void *));
    slab->head = NULL;

    if(!try_expand_slab(slab)) {
        panic("heap: insufficient memory");
        UNREACHABLE();
    }
}

void *malloc(size_t n)
{
    meta_t *meta;
    slab_t *slab;
    void *head_ptr;
    size_t npages;

    if((slab = find_matching_slab(n)) != NULL) {
        if(!slab->head && !try_expand_slab(slab))
            return NULL;
        head_ptr = slab->head;
        slab->head = slab->head[0];
        return head_ptr;
    }

    npages = get_page_count(n + 1);
    if((head_ptr = pmem_alloc_highmem(npages)) != NULL) {
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
    meta_t *meta;
    slab_t *slab;
    void **head_ptr;
    void *aligned_ptr;
    void *new_ptr;

    if(ptr) {
        if((new_ptr = malloc(n)) != NULL) {
            aligned_ptr = page_align_pointer(ptr);

            if(ptr != aligned_ptr) {
                slab = ((slab_t **)aligned_ptr)[0];

                memcpy(new_ptr, ptr, slab->blocksize);

                head_ptr = ptr;
                head_ptr[0] = slab->head;
                slab->head = head_ptr;

                return new_ptr;
            }

            meta = (meta_t *)((uintptr_t)aligned_ptr - PAGE_SIZE);

            if(meta->length < n)
                n = meta->length;
            memcpy(new_ptr, ptr, n);

            pmem_free_highmem(meta, meta->npages + 1);
            return new_ptr;
        }

        free(ptr);

        return NULL;
    }

    return malloc(n);
}

void free(void *restrict ptr)
{
    meta_t *meta;
    slab_t *slab;
    void **head_ptr;
    void *aligned_ptr;

    if(ptr) {
        aligned_ptr = page_align_pointer(ptr);

        if(ptr != aligned_ptr) {
            slab = ((slab_t **)aligned_ptr)[0];

            head_ptr = ptr;
            head_ptr[0] = slab->head;
            slab->head = head_ptr;

            /* FIXME: if this free() call was the one
             * that made a certain page fully free, we
             * may want to give the page back to the PMM. */
            return;
        }

        meta = (meta_t *)((uintptr_t)aligned_ptr - PAGE_SIZE);
        pmem_free_highmem(meta, meta->npages + 1);
        return;
    }
}

static void init_heap(void)
{
    size_t numslab = 8;
    size_t numpage = get_page_count(sizeof(slab_t) * (numslab + 1));

    if((slabs = pmem_alloc_highmem(numpage)) == NULL) {
        panic("heap: insufficient memory");
        UNREACHABLE();
    }

    memset(slabs, 0, sizeof(slab_t) * (numslab + 1));

    /* FIXME: use actual blocksize values
     * instead of blindly using PAGE_SIZE fractions */
    init_slab(&slabs[0], PAGE_SIZE / 512);
    init_slab(&slabs[1], PAGE_SIZE / 256);
    init_slab(&slabs[2], PAGE_SIZE / 128);
    init_slab(&slabs[3], PAGE_SIZE / 64);
    init_slab(&slabs[4], PAGE_SIZE / 32);
    init_slab(&slabs[5], PAGE_SIZE / 16);
    init_slab(&slabs[6], PAGE_SIZE / 8);
    init_slab(&slabs[7], PAGE_SIZE / 4);
}
early_initcall(heap, init_heap);
