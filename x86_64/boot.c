/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <limine.h>
#include <string.h>
#include <sys/boot.h>
#include <sys/debug.h>
#include <sys/pmalloc.h>

console_t boot_console = { 0 };
uintptr_t hhdm_offset = 0;
uintptr_t load_phys = 0;
uintptr_t load_virt = 0;

static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .response = NULL,
    .revision = 0,
};

static volatile struct limine_kernel_address_request kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .response = NULL,
    .revision = 0,
};

static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .response = NULL,
    .revision = 0,
};

static volatile struct limine_terminal_request terminal_request = {
    .id = LIMINE_TERMINAL_REQUEST,
    .response = NULL,
    .revision = 0,
};

static void boot_console_putchar(console_t *restrict console, int c)
{
    size_t i;
    char chr = c;
    for(i = 0; i < terminal_request.response->terminal_count; i++) {
        terminal_request.response->write(terminal_request.response->terminals[i], &chr, 1);
    }
}

static void init_boot(void)
{
    size_t i;
    const struct limine_memmap_entry *entry;

    if(terminal_request.response) {
        boot_console.con_next = NULL;
        boot_console.con_putchar = &boot_console_putchar;
        boot_console.con_unblank = NULL;
        boot_console.con_name = "limine_terminal";
        boot_console.con_data = NULL;
        register_console(&boot_console);
    }

    kassert(hhdm_request.response != NULL);
    kassert(memmap_request.response != NULL);
    kassert(kernel_address_request.response != NULL);

    hhdm_offset = hhdm_request.response->offset;
    load_phys = kernel_address_request.response->physical_base;
    load_virt = kernel_address_request.response->virtual_base;

    for(i = 0; i < memmap_request.response->entry_count; i++) {
        if(memmap_request.response->entries[i]->type == LIMINE_MEMMAP_USABLE) {
            entry = memmap_request.response->entries[i];
            add_memblock(entry->base, entry->length);
        }
    }
}
early_initcall(boot, init_boot);
