/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <string.h>
#include <sys/console.h>
#include <sys/initcall.h>
#include <sys/page.h>
#include <sys/pmem.h>
#include <sys/systm.h>
#include <x86_64/boot.h>

uintptr_t highmem_offset = 0;
uintptr_t kernel_base_phys = 0;
uintptr_t kernel_base_virt = 0;

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

void __used __noreturn kstart(void)
{
    size_t i;

    kprintf("init: starting version %s+%s", version, git_revision);

    if(terminal_request.response) {
        /* Until an fbcon-like driver takes over,
         * Limine's built-in terminal subsystem is
         * responsible for printing things to the framebuffer */
        register_console(&liminecon);
    }

    kassertf(hhdm_request.response, "init: highmem not available");
    kassertf(memmap_request.response, "init: memmap not available");
    kassertf(kernel_address_request.response, "init: kernel address not available");

    highmem_offset = hhdm_request.response->offset;
    kernel_base_phys = kernel_address_request.response->physical_base;
    kernel_base_virt = kernel_address_request.response->virtual_base;

    for(i = 0; i < memmap_request.response->entry_count; ++i) {
        if(memmap_request.response->entries[i]->type != LIMINE_MEMMAP_USABLE)
            continue;
        pmem_add_memblock(memmap_request.response->entries[i]->base, get_page_count(memmap_request.response->entries[i]->length));
    }

    /* UNDONE: should SMP and sched use initcalls? */
    for(i = 0; initcalls[i].func; initcalls[i++].func());

    panic("init: kstart has reached its end");
    UNREACHABLE();
}
