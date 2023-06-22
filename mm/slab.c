/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <mm/align.h>
#include <mm/pmm.h>
#include <mm/slab.h>
#include <stdbool.h>
#include <string.h>
#include <sys/panic.h>
#include <sys/printf.h>

struct slab {
    size_t sl_blocksize;
    void **sl_head;
};

static size_t num_slab = 0;
static struct slab *slabs = NULL;

static struct slab *find_slab(size_t n)
{
    size_t i;

    for(i = 0; i < num_slab; ++i) {
        if(slabs[i].sl_blocksize <= n)
            return &slabs[i];
        continue;
    }

    return NULL;
}

static bool expand_slab(struct slab *restrict sl)
{
    size_t i;
    size_t hsize;
    size_t obj_count;
    size_t obj_gap;

    kassert(sl->sl_blocksize / sizeof(void *) >= 1);
    kassert(sl->sl_blocksize % sizeof(void *) == 0);

    if((sl->sl_head = pmm_alloc_page_hhdm()) != NULL) {
        hsize = __align_ceil(sizeof(struct slab *), sl->sl_blocksize);

        sl->sl_head[0] = sl;
        sl->sl_head = (void **)((uintptr_t)sl->sl_head + hsize);

        /* Linked list will have obj_count new entries,
         * each of them pointing to the next one obj_gap bytes forward */
        obj_count = __align_floor((PAGE_SIZE - hsize), sl->sl_blocksize) / sl->sl_blocksize;
        obj_gap = sl->sl_blocksize / sizeof(void *);

        for(i = 1; i < obj_count; i++)
            sl->sl_head[obj_gap * (i - 1)] = &sl->sl_head[obj_gap * i];
        sl->sl_head[obj_gap * (obj_count - 1)] = NULL;

        return true;
    }

    return false;
}

static void setup_slab(struct slab *restrict sl, size_t blocksize)
{
    sl->sl_blocksize = __align_ceil(blocksize, sizeof(void *));
    sl->sl_head = NULL;
    kassertf(expand_slab(sl), "slab: insufficient memory");
}

void *slab_alloc(size_t n)
{
    struct slab *sl;
    void *head_ptr;

    if((sl = find_slab(n)) != NULL) {
        if(!sl->sl_head && !expand_slab(sl))
            return NULL;
        head_ptr = sl->sl_head;
        sl->sl_head = sl->sl_head[0];
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
    struct slab *sl;
    void **head_ptr;
    void *aligned_ptr;
    void *new_ptr;

    if(ptr) {
        if((new_ptr = slab_alloc(n)) != NULL) {
            aligned_ptr = page_align_pointer(ptr);

            if(ptr != aligned_ptr) {
                sl = ((struct slab **)aligned_ptr)[0];

                memcpy(new_ptr, ptr, sl->sl_blocksize);

                head_ptr = ptr;
                head_ptr[0] = sl->sl_head;
                sl->sl_head = head_ptr;

                return new_ptr;
            }
        }

        slab_free(ptr);

        return NULL;
    }

    return slab_alloc(n);
}

void slab_free(void *restrict ptr)
{
    struct slab *sl;
    void **head_ptr;
    void *aligned_ptr;

    if(ptr) {
        aligned_ptr = page_align_pointer(ptr);

        if(ptr != aligned_ptr) {
            sl = ((struct slab **)aligned_ptr)[0];

            head_ptr = ptr;
            head_ptr[0] = sl->sl_head;
            sl->sl_head = head_ptr;
        }
    }
}

static void init_slab(void)
{
    size_t npages;

    num_slab = 8;
    npages = get_page_count(sizeof(struct slab) * num_slab);

    if((slabs = pmm_alloc_hhdm(npages)) == NULL) {
        panic("slab: insufficient memory");
        unreachable();
    }

    memset(slabs, 0, npages * PAGE_SIZE);

    setup_slab(&slabs[0], PAGE_SIZE / 512);
    setup_slab(&slabs[1], PAGE_SIZE / 256);
    setup_slab(&slabs[2], PAGE_SIZE / 128);
    setup_slab(&slabs[3], PAGE_SIZE / 64);
    setup_slab(&slabs[4], PAGE_SIZE / 32);
    setup_slab(&slabs[5], PAGE_SIZE / 16);
    setup_slab(&slabs[6], PAGE_SIZE / 8);
    setup_slab(&slabs[7], PAGE_SIZE / 4);
}
core_initcall(slab, init_slab);
initcall_depend(slab, pmm);
