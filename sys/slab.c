/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <stdbool.h>
#include <string.h>
#include <sys/boot.h>
#include <sys/debug.h>
#include <sys/pmalloc.h>
#include <sys/slab.h>

typedef struct slab_s {
    size_t objsize;
    void **head;
} slab_t;

#define NSLABS 8
static slab_t slabs[NSLABS] = { 0 };

static slab_t *find_slab(size_t n)
{
    size_t i;
    for(i = 0; i < NSLABS; i++) {
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

    // Allocate a new page
    slab->head = pmalloc();

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

static void setup_slab(slab_t *restrict slab, size_t objsize)
{
    slab->head = NULL;
    slab->objsize = __align_ceil(objsize, sizeof(void *));

    if(!expand_slab(slab)) {
        // If there is not enough memory to set up basic
        // memory allocation structures, what's the point
        // for the kernel to continue functioning?
        panic("slab: insufficient memory");
    }
}

void *slab_alloc(size_t n)
{
    slab_t *slab = find_slab(n);
    void *head_ptr;

    if(slab) {
        if(!slab->head && !expand_slab(slab))
            return NULL;
        head_ptr = slab->head;
        slab->head = slab->head[0];
        return head_ptr;
    }

    return NULL;
}

void *slab_calloc(size_t count, size_t n)
{
    void *ptr = slab_alloc(count * n);
    if(!ptr)
        return NULL;
    return memset(ptr, 0, count * n);
}

void *slab_realloc(void *restrict ptr, size_t n)
{
    slab_t *slab;
    void **head_ptr;
    void *aligned_ptr;
    void *new_ptr;

    if(ptr) {
        new_ptr = slab_alloc(n);

        if(new_ptr) {
            aligned_ptr = page_align_pointer(ptr);
            slab = ((slab_t **)aligned_ptr)[0];
            memcpy(new_ptr, ptr, slab->objsize);

            head_ptr = ptr;
            head_ptr[0] = slab->head;
            slab->head = head_ptr;

            return new_ptr;
        }
    }

    return slab_alloc(n);
}

void slab_free(void *restrict ptr)
{
    slab_t *slab;
    void **head_ptr;
    void *aligned_ptr;

    if(ptr) {
        aligned_ptr = page_align_pointer(ptr);
        slab = ((slab_t **)aligned_ptr)[0];

        head_ptr = ptr;
        head_ptr[0] = slab->head;
        slab->head = head_ptr;

        // FIXME: if this slab_free() call was the one
        // that made a certain page fully slab_free, we
        // may want to give the page back to the PMM.
        return;
    }
}

static void init_slab(void)
{
    // FIXME: use actual objsize values
    setup_slab(&slabs[0], PAGE_SIZE / 512);
    setup_slab(&slabs[1], PAGE_SIZE / 256);
    setup_slab(&slabs[2], PAGE_SIZE / 128);
    setup_slab(&slabs[3], PAGE_SIZE / 64);
    setup_slab(&slabs[4], PAGE_SIZE / 32);
    setup_slab(&slabs[5], PAGE_SIZE / 16);
    setup_slab(&slabs[6], PAGE_SIZE / 8);
    setup_slab(&slabs[7], PAGE_SIZE / 4);
}
early_initcall(slab, init_slab);
initcall_dependency(slab, pmalloc);
