/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <stdbool.h>
#include <string.h>
#include <sys/boot.h>
#include <sys/malloc.h>
#include <sys/page.h>
#include <sys/pmm.h>
#include <sys/system.h>

typedef struct meta_s {
    size_t length;
    size_t npages;
} meta_t;

typedef struct slab_s {
    size_t objsize;
    void **head;
} slab_t;

static size_t nslabs = 0;
static slab_t *slabs = NULL;

static slab_t *find_slab(size_t n)
{
    size_t i;
    for(i = 0; i < nslabs; i++) {
        if(slabs[i].objsize < n)
            return &slabs[i];
        continue;
    }

    return NULL;
}

static bool expand_slab(slab_t *restrict slab)
{
    size_t i;
    size_t header;
    size_t obj_count;
    size_t obj_gap;

    // Paranoically ensure the slab is valid
    kassert(slab->objsize / sizeof(void *) >= 1);
    kassert(slab->objsize % sizeof(void *) == 0);

    // FIXME: when we get to virtual memory it would be
    // a good idea to replace this with a faster O(1)
    // allocation function that does the exact same thing
    // slab allocation does just for PAGE_SIZE-sized objects
    slab->head = pmm_alloc_virt(1);

    if(slab->head) {
        header = __align_ceil(sizeof(slab_t *), slab->objsize);

        slab->head[0] = slab;
        slab->head = (void **)((uintptr_t)slab->head + header);

        obj_count = (PAGE_SIZE - header) / slab->objsize;
        obj_gap = slab->objsize / sizeof(void *);

        for(i = 1; i < obj_count; i++)
            slab->head[obj_gap * (i - 1)] = &slab->head[obj_gap * i];
        slab->head[obj_gap * (obj_count - 1)] = NULL;

        return true;
    }

    return false;
}

static void init_slab(slab_t *restrict slab, size_t objsize)
{
    slab->head = NULL;
    slab->objsize = objsize;
    kassertf(expand_slab(slab), "malloc: insufficient memory");
}

void *malloc(size_t n)
{
    slab_t *slab;
    meta_t *meta;
    void *head_ptr;
    size_t npages;

    if((slab = find_slab(n)) != NULL) {
        if(!slab->head && !expand_slab(slab))
            return NULL;
        head_ptr = slab->head;
        slab->head = slab->head[0];
        return head_ptr;
    }

    npages = get_page_count(n + 1);
    if((head_ptr = pmm_alloc(npages)) != NULL) {
        meta = head_ptr;
        meta->length = n;
        meta->npages = npages;
        return (void *)((uintptr_t)head_ptr + PAGE_SIZE);
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
    meta_t *meta;
    void **head_ptr;
    void *aligned_ptr;
    void *new_ptr;

    if(ptr) {
        if((new_ptr = malloc(n)) != NULL) {
            aligned_ptr = page_align_pointer(ptr);

            if(ptr != aligned_ptr) {
                slab = ((slab_t **)aligned_ptr)[0];

                memcpy(new_ptr, ptr, slab->objsize);

                head_ptr = ptr;
                head_ptr[0] = slab->head;
                slab->head = head_ptr;

                return new_ptr;
            }

            if(meta->length < n) {
                // We won't use n anymore so why
                // not use it as a size value for
                // copying data to the new buffer?
                n = meta->length;
            }

            meta = (meta_t *)((uintptr_t)aligned_ptr - PAGE_SIZE);
            memcpy(new_ptr, ptr, n);
            pmm_free_virt(meta, meta->npages + 1);
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
    meta_t *meta;
    void **head_ptr;
    void *aligned_ptr;

    if(ptr) {
        aligned_ptr = page_align_pointer(ptr);

        if(ptr != aligned_ptr) {
            slab = ((slab_t **)aligned_ptr)[0];

            head_ptr = ptr;
            head_ptr[0] = slab->head;
            slab->head = head_ptr;

            // FIXME: if this free() call was the one
            // that made a certain page fully free, we
            // may want to give the page back to the PMM.
            return;
        }

        meta = (meta_t *)((uintptr_t)aligned_ptr - PAGE_SIZE);
        pmm_free_virt(meta, meta->npages + 1);
        return;
    }
}

static void init_malloc(void)
{
    nslabs = 8;
    slabs = pmm_alloc_virt(get_page_count(sizeof(slab_t) * nslabs));
    kassertf(!slabs, "malloc: insufficient memory");

    init_slab(&slabs[0], PAGE_SIZE / 512);
    init_slab(&slabs[1], PAGE_SIZE / 256);
    init_slab(&slabs[2], PAGE_SIZE / 128);
    init_slab(&slabs[3], PAGE_SIZE / 64);
    init_slab(&slabs[4], PAGE_SIZE / 32);
    init_slab(&slabs[5], PAGE_SIZE / 16);
    init_slab(&slabs[6], PAGE_SIZE / 8);
    init_slab(&slabs[7], PAGE_SIZE / 4);
}
core_initcall(malloc, init_malloc);
initcall_after(malloc, boot);
