#include <core/assert.h>
#include <core/errno.h>
#include <core/linker.h>
#include <core/panic.h>
#include <core/spinlock.h>
#include <init/initcall.h>
#include <lib/sprintf.h>
#include <lib/string.h>
#include <lib/strings.h>
#include <mm/malloc.h>
#include <mm/page.h>
#include <mm/pslab.h>
#include <mm/slab.h>
#include <mm/vmm.h>
#include <mm/vprot.h>

#define MALLOC_ALIGNMENT sizeof(uintptr_t)

#define BLOCK_TYPE_FREE 0x0000U
#define BLOCK_TYPE_USED 0x0001U

struct memblock {
    struct memblock* prev;
    struct memblock* next;
    unsigned short type;
    uintptr_t address;
    size_t length;
};

static struct memblock* head = nullptr;
static struct memblock* tail = nullptr;
static uintptr_t heap_end = 0;
static spinlock_type heap_lock = SPINLOCK_INIT;

static struct memblock* find_free_block(size_t sz)
{
    struct memblock* it;

    for(it = head; it; it = it->next) {
        if(it->type == BLOCK_TYPE_FREE && it->length >= sz) {
            return it;
        }
    }

    return nullptr;
}

static struct memblock* extend_heap(size_t sz)
{
    size_t i;
    uintptr_t phys;
    uintptr_t virt;

    size_t npages = page_count(sz);
    struct memblock* block = slab_alloc(sizeof(struct memblock));

    if(block == nullptr) {
        return nullptr;
    }

    block->address = heap_end;
    block->length = npages * PAGE_SIZE;
    block->type = BLOCK_TYPE_FREE;
    block->prev = tail;
    block->next = nullptr;

    for(i = 0; i < npages; ++i) {
        phys = pslab_alloc();

        if(phys == 0) {
            goto error_cleanup;
        }

        virt = block->address + i * PAGE_SIZE;
        int retval = vmm_map(&sys_pagemap, virt, phys, VPROT_RW);

        if(retval < 0) {
            pslab_free(phys);
            goto error_cleanup;
        }
    }

    heap_end += block->length;

    if(tail) {
        tail->next = block;
        tail = block;
    }
    else {
        head = block;
        tail = block;
    }

    return block;

error_cleanup:
    while(i--) {
        virt = block->address + i * PAGE_SIZE;
        pslab_free(vmm_resolve(&sys_pagemap, virt));
        vmm_unmap(&sys_pagemap, virt);
    }

    slab_free(block);

    return nullptr;
}

static void split_block(struct memblock* restrict block, size_t sz)
{
    size_t aligned_sz = align_ceil(sz, MALLOC_ALIGNMENT);
    size_t remainder = block->length - aligned_sz;

    if(remainder < 4 * sizeof(uintptr_t)) {
        return;
    }

    struct memblock* new_block = slab_alloc(sizeof(struct memblock));

    if(new_block == nullptr) {
        return;
    }

    new_block->address = block->address + aligned_sz;
    new_block->length = remainder;
    new_block->type = BLOCK_TYPE_FREE;
    new_block->prev = block;
    new_block->next = block->next;

    if(block->next) {
        block->next->prev = new_block;
    }
    else {
        tail = new_block;
    }

    block->next = new_block;
    block->length = aligned_sz;
}

static struct memblock* find_block_at(uintptr_t address)
{
    struct memblock* it;

    for(it = head; it; it = it->next) {
        if(it->address == address) {
            return it;
        }
    }

    return nullptr;
}

static void coalesce(struct memblock* restrict block)
{
    struct memblock* next_block;
    struct memblock* prev_block;

    if(block->next && block->next->type == BLOCK_TYPE_FREE) {
        next_block = block->next;

        block->length += next_block->length;
        block->next = next_block->next;

        if(next_block->next) {
            next_block->next->prev = block;
        }
        else {
            tail = block;
        }

        slab_free(next_block);
    }

    if(block->prev && block->prev->type == BLOCK_TYPE_FREE) {
        prev_block = block->prev;

        prev_block->length += block->length;
        prev_block->next = block->next;

        if(block->next) {
            block->next->prev = prev_block;
        }
        else {
            tail = prev_block;
        }

        slab_free(block);
    }
}

void* kmalloc(size_t sz)
{
    if(sz == 0) {
        return nullptr;
    }

    spinlock_acquire(&heap_lock);

    size_t aligned_sz = align_ceil(sz, MALLOC_ALIGNMENT);
    struct memblock* block = find_free_block(aligned_sz);

    if(block == nullptr) {
        block = extend_heap(aligned_sz);

        if(block == nullptr) {
            spinlock_release(&heap_lock);

            return nullptr;
        }
    }

    split_block(block, aligned_sz);

    block->type = BLOCK_TYPE_USED;

    spinlock_release(&heap_lock);

    return (void*)(block->address);
}

void* kcalloc(size_t count, size_t sz)
{
    size_t total = count * sz;
    void* ptr = kmalloc(total);

    if(ptr) {
        memset(ptr, 0, total);
    }

    return ptr;
}

void* krealloc(void* restrict ptr, size_t sz)
{
    if(ptr == nullptr) {
        return kmalloc(sz);
    }

    if(sz == 0) {
        kfree(ptr);

        return nullptr;
    }

    struct memblock* block = find_block_at((uintptr_t)(ptr));

    if(block == nullptr) {
        return nullptr;
    }

    size_t aligned_sz = align_ceil(sz, MALLOC_ALIGNMENT);

    if(block->length >= aligned_sz) {
        return ptr;
    }

    void* new_ptr = kmalloc(aligned_sz);

    if(new_ptr == nullptr) {
        return nullptr;
    }

    memcpy(new_ptr, ptr, block->length);
    kfree(ptr);

    return new_ptr;
}

void kfree(void* restrict ptr)
{
    if(ptr == nullptr) {
        return;
    }

    struct memblock* block = find_block_at((uintptr_t)(ptr));

    if(block == nullptr || block->type == BLOCK_TYPE_FREE) {
        panic("kfree: invalid or double free of %p", ptr);
        unreachable();
    }

    block->type = BLOCK_TYPE_FREE;

    spinlock_acquire(&heap_lock);

    coalesce(block);

    spinlock_release(&heap_lock);
}

char* kstrdup(const char* restrict string)
{
    if(string == nullptr) {
        return nullptr;
    }

    size_t size = 1 + strlen(string);
    char* result = kmalloc(size);

    if(result == nullptr) {
        return nullptr;
    }

    memset(result, 0, size);
    kstrncpy(result, string, size - 1);

    return result;
}

char* kasprintf(const char* restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char* result = kvasprintf(fmt, ap);
    va_end(ap);

    return result;
}

char* kvasprintf(const char* restrict fmt, va_list ap)
{
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int length = vsnprintf(nullptr, 0, fmt, ap);

    if(length < 0) {
        va_end(ap_copy);
        return nullptr;
    }

    char* result = kcalloc(1, length + 1);

    if(result == nullptr) {
        va_end(ap_copy);
        return nullptr;
    }

    memset(result, 0, length + 1);
    vsnprintf(result, length, fmt, ap_copy);

    va_end(ap_copy);

    return result;
}

define_initcall(init_malloc, INITCALL_STAGE_BSP_HEAP, INITCALL_ORDER_FIRST)
{
    heap_end = PAGE_SIZE + page_align_up((uintptr_t)(kernel_end));
}
