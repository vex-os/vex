/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/boot.h>
#include <kan/console.h>
#include <kan/debug.h>
#include <kan/errno.h>
#include <kan/kprintf.h>
#include <kan/pmem.h>
#include <kan/symbol.h>
#include <stddef.h>
#include <string.h>

static volatile struct limine_bootloader_info_request info_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST,
    .response = NULL,
    .revision = 0,
};

static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .response = NULL,
    .revision = 0,
};

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

static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .response = NULL,
    .revision = 0,
};

static volatile struct limine_terminal_request terminal_request = {
    .id = LIMINE_TERMINAL_REQUEST,
    .response = NULL,
    .revision = 0,
};

static uintptr_t hhdm_offset = 0;
static uintptr_t kernel_address_phys = 0;
static uintptr_t kernel_address_virt = 0;

static void boot_console_puts(console_t *restrict con, const char *restrict s)
{
    size_t i;
    size_t n = strlen(s);
    struct limine_terminal *term;
    for(i = 0; i < terminal_request.response->terminal_count; i++) {
        term = terminal_request.response->terminals[i];
        terminal_request.response->write(term, s, n);
    }
}

static console_t boot_console = {
    .name = "boot_console",
    .puts_fn = &boot_console_puts,
    .next = NULL,
};

uintptr_t get_hhdm_offset(void)
{
    return hhdm_offset;
}
EXPORT_SYMBOL(get_hhdm_offset);

uintptr_t get_kernel_address_phys(void)
{
    return kernel_address_phys;
}
EXPORT_SYMBOL(get_kernel_address_phys);

uintptr_t get_kernel_address_virt(void)
{
    return kernel_address_virt;
}
EXPORT_SYMBOL(get_kernel_address_virt);

const struct limine_memmap_response *get_memmap(void)
{
    return memmap_request.response;
}

const struct limine_module_response *get_modules(void)
{
    return module_request.response;
}

const struct limine_framebuffer_response *get_framebuffers(void)
{
    return framebuffer_request.response;
}

void reclaim_bootloader_memory(void)
{
    size_t i;
    size_t fsize = 0;
    const struct limine_memmap_entry *entry;

    if(memmap_request.response) {
        for(i = 0; i < memmap_request.response->entry_count; i++) {
            entry = memmap_request.response->entries[i];
            if(entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
                pmfree(page_align_address(entry->base), get_page_count(entry->length));
                fsize += entry->length;
            }
        }
    }

    pr_inform("boot_info: reclaimed %zu KiB", fsize);
}

void unregister_boot_console(void)
{
    if(!terminal_request.response || !terminal_request.response->terminal_count)
        return;
    unregister_console(&boot_console);
}

static int init_boot_console(void)
{
    if(terminal_request.response && terminal_request.response->terminal_count)
        return register_console(&boot_console);
    return ENODEV;
}
initcall_tier_0(boot_console, init_boot_console);

static int init_boot_info(void)
{
    const char *name;
    const char *version;

    if(info_request.response) {
        name = info_request.response->name;
        version = info_request.response->version;
    }
    else {
        name = "unknown";
        version = "0.0.0";
    }

    pr_inform("boot_info: %s %s", name, version);

    panic_if(!hhdm_request.response, "boot_info: no hhdm_response");
    panic_if(!kernel_address_request.response, "boot_info: no kernel_address_response");
    panic_if(!memmap_request.response, "boot_info: no memmap_response");

    hhdm_offset = hhdm_request.response->offset;
    kernel_address_phys = kernel_address_request.response->physical_base;
    kernel_address_virt = kernel_address_request.response->virtual_base;

    return EOK;
}
initcall_tier_0(boot_info, init_boot_info);
