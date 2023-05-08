/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <string.h>
#include <sys/console.h>
#include <sys/initcall.h>
#include <sys/page.h>
#include <sys/pmem.h>
#include <sys/systm.h>
#include <x86_64/limine.h>

static volatile struct limine_5_level_paging_request PML5_request = {
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

uintptr_t highmem_offset = 0;
uintptr_t kernel_base_phys = 0;
uintptr_t kernel_base_virt = 0;
unsigned short bootflags = 0;

static void bootcon_putchar(console_t *restrict console, int c)
{
    size_t i;
    char val = c;
    struct limine_terminal *term;

    for(i = 0; i < terminal_request.response->terminal_count; i++) {
        term = terminal_request.response->terminals[i];
        terminal_request.response->write(term, &val, 1);
    }
}

static console_t bootcon = {
    .next = NULL,
    .putchar = &bootcon_putchar,
    .unblank = NULL,
    .ident = "limine_terminal",
    .data = NULL,
};

void __used __noreturn kstart_limine(void)
{
    size_t i;

    /* The kernel always starts its life with this message */
    kprintf("starting version %s+%s", version, git_revision);

    if(terminal_request.response) {
        /* Until fbconsole takes over, this
         * is going to be THE framebuffer console */
        register_console(&bootcon);
    }

    kassert(hhdm_request.response != NULL);
    kassert(memmap_request.response != NULL);
    kassert(kernel_address_request.response != NULL);

    highmem_offset = hhdm_request.response->offset;
    kernel_base_phys = kernel_address_request.response->physical_base;
    kernel_base_virt = kernel_address_request.response->virtual_base;
    bootflags = 0;

    /* x86 long mode */
    bootflags |= BOOTFLAG_VMM_L3;
    bootflags |= BOOTFLAG_VMM_L4;

    if(PML5_request.response) {
        /* x86 5-level paging */
        bootflags |= BOOTFLAG_VMM_L5;
    }

    for(i = 0; i < memmap_request.response->entry_count; ++i) {
        if(memmap_request.response->entries[i]->type != LIMINE_MEMMAP_USABLE)
            continue;
        pmem_add_memblock(memmap_request.response->entries[i]->base, get_page_count(memmap_request.response->entries[i]->length));
    }

    /* UNDONE: should SMP and sched use initcalls? */
    for(i = 0; initcalls[i].func; initcalls[i++].func());

    panic("boot: nothing else to do!");
    UNREACHABLE();
}
