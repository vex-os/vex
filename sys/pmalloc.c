/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <string.h>
#include <sys/boot.h>
#include <sys/kprintf.h>
#include <sys/pmalloc.h>

static void **pmm_head = NULL;
static size_t total_memory = 0;
static size_t used_memory = 0;

size_t get_total_memory(void)
{
    return total_memory;
}

size_t get_used_memory(void)
{
    return used_memory;
}

uintptr_t pmalloc(void)
{
    void *head_ptr;

    if(pmm_head) {
        head_ptr = pmm_head;
        pmm_head = pmm_head[0];
        used_memory += PAGE_SIZE;
        return (uintptr_t)head_ptr - hhdm_offset;
    }

    // Too bad!
    return 0;
}

void *pmalloc_virt(void)
{
    void *head_ptr;

    if(pmm_head) {
        head_ptr = pmm_head;
        pmm_head = pmm_head[0];
        used_memory += PAGE_SIZE;
        return head_ptr;
    }

    // Too bad!
    return NULL;
}

void pmfree(uintptr_t address)
{
    void **head_ptr;

    if(address) {
        head_ptr = (void **)(address + hhdm_offset);
        head_ptr[0] = pmm_head;
        pmm_head = head_ptr;
        used_memory -= PAGE_SIZE;
    }
}

void pmfree_virt(void *restrict ptr)
{
    void **head_ptr;

    if(ptr) {
        head_ptr = ptr;
        head_ptr[0] = pmm_head;
        pmm_head = head_ptr;
        used_memory -= PAGE_SIZE;
    }
}

static void init_pmalloc(void)
{
    size_t i;
    size_t npages;
    uintptr_t address;
    void **head_ptr;
    const boot_memmap_t *entry;

    pmm_head = NULL;
    total_memory = 0;
    used_memory = 0;

    for(entry = boot_memmap; entry->type != BOOT_MEMMAP_LAST; ++entry) {
        if(entry->type == BOOT_MEMMAP_USABLE) {
            npages = get_page_count(entry->length);
            address = page_align_address(entry->address);
            total_memory += npages * PAGE_SIZE;

            for(i = 0; i < npages; i++) {
                head_ptr = (void **)(hhdm_offset + address + (i * PAGE_SIZE));
                head_ptr[0] = pmm_head;
                pmm_head = head_ptr;
            }
        }

        kprintf("pmalloc: type=%#04hX, address=%p, length=%zu", entry->type, (void *)entry->address, entry->length);
    }

    kprintf("pmalloc: total memory: %zu KiB (%zu KB)", total_memory / 1024, total_memory / 1000);
}
early_initcall(pmalloc, init_pmalloc);
initcall_dependency(pmalloc, boot);
