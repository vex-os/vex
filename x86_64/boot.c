/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <string.h>
#include <sys/console.h>
#include <sys/initcall.h>
#include <sys/page.h>
#include <sys/pmem.h>
#include <sys/systm.h>
#include <x86_64/limine.h>

static volatile struct limine_5_level_paging_request five_level_paging_request = {
    .id = LIMINE_5_LEVEL_PAGING_REQUEST,
    .revision = 0,
    .response = NULL,
};

static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = NULL,
};

static volatile struct limine_kernel_address_request kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0,
    .response = NULL,
};

static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
    .response = NULL,
};

static volatile struct limine_terminal_request terminal_request = {
    .id = LIMINE_TERMINAL_REQUEST,
    .revision = 0,
    .response = NULL,
    .callback = NULL,
};

memmap_t *memmap = NULL;
size_t memmap_size = 0;
uintptr_t highmem_offset = 0;
uintptr_t kernel_base_phys = 0;
uintptr_t kernel_base_virt = 0;
unsigned short bootflags = 0;

static void liminecon_putchar(console_t *restrict console, int c)
{
    size_t i;
    char value = c;
    struct limine_terminal *term;

    for(i = 0; i < terminal_request.response->terminal_count; ++i) {
        term = terminal_request.response->terminals[i];
        terminal_request.response->write(term, &value, 1);
    }
}

static console_t liminecon = {
    .next = NULL,
    .putchar = &liminecon_putchar,
    .unblank = NULL,
    .ident = "liminecon",
    .data = NULL,
};

static void *boot_alloc(size_t n)
{
    size_t i;
    size_t npages = get_page_count(n);
    size_t nbytes = PAGE_SIZE * npages;
    struct limine_memmap_entry *entry;
    uintptr_t address;

    for(i = 0; i < memmap_request.response->entry_count; ++i) {
        entry = memmap_request.response->entries[i];

        if((entry->type == LIMINE_MEMMAP_USABLE) && (entry->length >= nbytes)) {
            address = entry->base;
            entry->base += nbytes;
            entry->length -= nbytes;
            return (void *)(address + highmem_offset);
        }
    }

    return NULL;
}

/* Referenced in ldscript.x86_64.lds */
void __used __noreturn kstart(void)
{
    size_t i;

    kprintf("starting version %s+%s", version, git_revision);

    if(terminal_request.response) {
        /* Until an fbcon-like driver takes over,
         * Limine's built-in terminal subsystem is
         * responsible for printing things to the framebuffer */
        register_console(&liminecon);
    }

    kassertf(hhdm_request.response, "boot: highmem not available");
    kassertf(memmap_request.response, "boot: memmap not available");
    kassertf(kernel_address_request.response, "boot: kernel address not available");

    highmem_offset = hhdm_request.response->offset;
    kernel_base_phys = kernel_address_request.response->physical_base;
    kernel_base_virt = kernel_address_request.response->virtual_base;
    bootflags = 0x0000;

    /* x86-64 long mode */
    bootflags |= BOOTFLAG_VMM_PML3;
    bootflags |= BOOTFLAG_VMM_PML4;

    if(five_level_paging_request.response) {
        /* x86-64 five-level paging */
        bootflags |= BOOTFLAG_VMM_PML5;
    }

    memmap_size = memmap_request.response->entry_count;
    memmap = boot_alloc(sizeof(memmap_t) * memmap_size);
    kassertf(memmap, "boot: insufficient memory");

    for(i = 0; i < memmap_size; ++i) {
        switch(memmap_request.response->entries[i]->type) {
            case LIMINE_MEMMAP_USABLE:
                memmap[i].type = MEMMAP_USABLE;
                break;
            case LIMINE_MEMMAP_RESERVED:
                memmap[i].type = MEMMAP_RESERVED;
                break;
            case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
                memmap[i].type = MEMMAP_ACPI_DATA;
                break;
            case LIMINE_MEMMAP_ACPI_NVS:
                memmap[i].type = MEMMAP_ACPI_NVS;
                break;
            case LIMINE_MEMMAP_BAD_MEMORY:
                memmap[i].type = MEMMAP_BAD_MEMORY;
                break;
            case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
                memmap[i].type = MEMMAP_BOOTLOADER;
                break;
            case LIMINE_MEMMAP_KERNEL_AND_MODULES:
                memmap[i].type = MEMMAP_KERNEL;
                break;
            case LIMINE_MEMMAP_FRAMEBUFFER:
                memmap[i].type = MEMMAP_FRAMEBUFFER;
                break;
            default:
                memmap[i].type = MEMMAP_RESERVED;
                break;
        }

        memmap[i].address = memmap_request.response->entries[i]->base;
        memmap[i].length = memmap_request.response->entries[i]->length;

        if(memmap[i].type == MEMMAP_USABLE) {
            /* Initializing usable memory regions right now
             * allows for whatever subsystem that is initialized
             * through initcalls to allocate memory without a fuss */
            pmem_add_memblock(memmap[i].address, get_page_count(memmap[i].length));
        }

        kprintf("memmap: type=%04hX, address=%p, length=%zu", memmap[i].type, (void *)memmap[i].address, memmap[i].length);
    }

    /* UNDONE: should SMP and sched use initcalls? */
    for(i = 0; initcalls[i].func; initcalls[i++].func());

    panic("boot: kstart has reached its end");
    UNREACHABLE();
}
