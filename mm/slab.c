// SPDX-License-Identifier: BSD-3-Clause

#include <core/assert.h>
#include <core/kprintf.h>
#include <core/panic.h>
#include <init/initfn.h>
#include <lib/string.h>
#include <mm/page.h>
#include <mm/pdma.h>
#include <mm/pslab.h>

#define SLAB_BLOCKSIZE(index) ((size_t)((1U << (index)) * sizeof(void*)))

struct slab {
    size_t blocksize;
    void** head;
};

static size_t slabs_count = 0U;
static struct slab* slabs = nullptr;

static struct slab* find_slab(size_t sz)
{
    size_t i;

    // Try to find the exact size match
    for(i = 0U; i < slabs_count; ++i) {
        if(slabs[i].blocksize == sz)
            return &slabs[i];
        continue;
    }

    // Give up and find anything that fits
    for(i = 0U; i < slabs_count; ++i) {
        if(slabs[i].blocksize >= sz)
            return &slabs[i];
        continue;
    }

    return nullptr;
}

static bool expand_slab(struct slab* restrict sl)
{
    size_t i;
    size_t header;
    size_t count, gap;

    kassert((sl->blocksize / sizeof(void*)) >= 1U);
    kassert((sl->blocksize % sizeof(void*)) == 0U);

    sl->head = pslab_alloc_virt();

    if(sl->head) {
        header = align_ceil(sizeof(struct slab*), sl->blocksize);

        sl->head[0] = sl;
        sl->head = (void**)((uintptr_t)(sl->head) + header);

        // Each page contains a chunk of the linked list
        // prepended with a header that takes at least a single
        // block of sl->blocksize bytes off the available space
        count = align_floor((PAGE_SIZE - header), sl->blocksize) / sl->blocksize;
        gap = sl->blocksize / sizeof(void*);

        for(i = 1U; i < count; ++i)
            sl->head[gap * (i - 1)] = &sl->head[gap * i];
        sl->head[gap * (count - 1)] = nullptr;

        return true;
    }

    return false;
}

void* __nodiscard slab_alloc(size_t sz)
{
    void** head_ptr;
    struct slab* sl = find_slab(sz);

    if(sl) {
        if(!sl->head && !expand_slab(sl)) {
            return nullptr;
        }

        head_ptr = sl->head;
        sl->head = head_ptr[0];
        return head_ptr;
    }

    return nullptr;
}

void* __nodiscard slab_calloc(size_t count, size_t sz)
{
    void* ptr = slab_alloc(count * sz);
    if(ptr == nullptr)
        return nullptr;
    return memset(ptr, 0, count * sz);
}

void* __nodiscard slab_realloc(void* restrict ptr, size_t sz)
{
    struct slab* sl;
    void** head_ptr;
    void* new_ptr = slab_alloc(sz);

    if(ptr == nullptr) {
        // Just return the new thing
        // without any memory copying
        return new_ptr;
    }

    sl = ((struct slab**)(page_align_ptr(ptr)))[0];

    memcpy(new_ptr, ptr, sl->blocksize);

    head_ptr = ptr;
    head_ptr[0] = sl->head;
    sl->head = new_ptr;

    return new_ptr;
}

void slab_free(void* restrict ptr)
{
    struct slab* sl;
    void** head_ptr;

    if(ptr) {
        sl = ((struct slab**)(page_align_ptr(ptr)))[0];

        head_ptr = ptr;
        head_ptr[0] = sl->head;
        sl->head = head_ptr;
    }
}

static int init_slab(void)
{
    size_t i;
    size_t npages;

    slabs_count = 0U;

    while(SLAB_BLOCKSIZE(slabs_count) <= (PAGE_SIZE / 4)) {
        slabs_count += 1U;
    }

    npages = page_count(slabs_count * sizeof(struct slab));
    slabs = pdma_alloc_virt(npages);

    if(slabs == nullptr) {
        panic("slab: out of memory [pdma_alloc %zu pages]", npages);
        unreachable();
    }

    for(i = 0U; i < slabs_count; ++i) {
        slabs[i].blocksize = SLAB_BLOCKSIZE(i);
        slabs[i].head = nullptr;

        if(!expand_slab(&slabs[i])) {
            panic("slab: out of memory [slab %zu:%zu]", i, slabs[i].blocksize);
            unreachable();
        }

        kprintf(KP_NOTICE, "slab: slab[%zu] head=%p blocksize=[%zu]", i, slabs[i].head, slabs[i].blocksize);
    }

    return 0;
}

DEFINE_INITFN(init_slab, INITFN_STAGE_SLAB, INITFN_ORDER_MIDDLE);
