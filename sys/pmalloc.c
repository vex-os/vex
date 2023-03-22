/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <string.h>
#include <sys/boot.h>
#include <sys/kprintf.h>
#include <sys/pmalloc.h>

static void **pmm_head = NULL;
static size_t total_memory = 0;
static size_t used_memory = 0;

void add_memblock(uintptr_t address, size_t n)
{
    size_t i;
    size_t npages;
    void **head_ptr;

    address = page_align_address(address);
    npages = get_page_count(n);
    total_memory += npages * PAGE_SIZE;

    for(i = 0; i < npages; i++) {
        head_ptr = (void **)(hhdm_offset + address + (i * PAGE_SIZE));
        head_ptr[0] = pmm_head;
        pmm_head = head_ptr;
    }
}

size_t get_total_memory(void)
{
    return total_memory;
}

size_t get_used_memory(void)
{
    return used_memory;
}

void *pmalloc(void)
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

void pmfree(void *restrict ptr)
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
    // pmalloc is essntially a portable part of bootloader
    // abstraction layer, so when init_pmalloc is called, memory
    // blocks are already set up (add_memblock is called by init_boot)
    kprintf("pmalloc: %zu KiB (%zu KB) free", total_memory / 1024, total_memory / 1000);
}
early_initcall(pmalloc, init_pmalloc);
initcall_dependency(pmalloc, boot);
