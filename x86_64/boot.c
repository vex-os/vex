/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <limine.h>
#include <kan/errno.h>
#include <machine/boot.h>
#include <string.h>
#include <sys/debug.h>
#include <sys/kprintf.h>
#include <sys/pmem.h>

// Required for hhdm_offset
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = NULL,
};

// Required for kernel_base_phys and kernel_base_virt
static volatile struct limine_kernel_address_request kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0,
    .response = NULL,
};

// Required for iterate_memmap_t and iterate_memmap()
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
    .response = NULL,
};

// Required for iterate_modules_t and iterate_modules()
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0,
    .response = NULL,
};

// Required for bootloader console
static volatile struct limine_terminal_request terminal_request = {
    .id = LIMINE_TERMINAL_REQUEST,
    .revision = 0,
    .response = NULL,
    .callback = NULL,
};

uintptr_t hhdm_offset = 0;
uintptr_t kernel_base_phys = 0;
uintptr_t kernel_base_virt = 0;

// Bootloader console/limine_terminal
static console_t boot_console = { 0 };

static void boot_console_putchar(console_t *restrict console, int c)
{
    size_t i;
    char value = c;
    struct limine_terminal *term;

    for(i = 0; i < terminal_request.response->terminal_count; i++) {
        term = terminal_request.response->terminals[i];
        terminal_request.response->write(term, &value, 1);
    }
}

void invalidate_boot_iterables(void)
{
    if(terminal_request.response) {
        // Console-related callbacks are located
        // within memory that just has been reclaimed.
        unregister_console(&boot_console);
    }

    hhdm_request.response = NULL;
    kernel_address_request.response = NULL;
    memmap_request.response = NULL;
    module_request.response = NULL;
    terminal_request.response = NULL;
}

int iterate_memmap(iterate_memmap_t callback, void *restrict arg)
{
    size_t i;
    unsigned short type;
    const struct limine_memmap_entry *it;

    if(memmap_request.response) {
        for(i = 0; i < memmap_request.response->entry_count; i++) {
            it = memmap_request.response->entries[i];

            switch(it->type) {
                case LIMINE_MEMMAP_USABLE:
                    type = MEMMAP_USABLE;
                    break;
                case LIMINE_MEMMAP_RESERVED:
                    type = MEMMAP_RESERVED;
                    break;
                case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
                    type = MEMMAP_ACPI_DATA;
                    break;
                case LIMINE_MEMMAP_ACPI_NVS:
                    type = MEMMAP_ACPI_NVS;
                    break;
                case LIMINE_MEMMAP_BAD_MEMORY:
                    type = MEMMAP_BAD_MEMORY;
                    break;
                case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
                    type = MEMMAP_BOOTLOADER;
                    break;
                case LIMINE_MEMMAP_KERNEL_AND_MODULES:
                    type = MEMMAP_KERNEL;
                    break;
                case LIMINE_MEMMAP_FRAMEBUFFER:
                    type = MEMMAP_FRAMEBUFFER;
                    break;
                default:
                    type = MEMMAP_RESERVED;
                    break;
            }

            callback(it->base, it->length, type, arg);
        }

        return 0;
    }

    return EINVAL;
}

int iterate_modules(iterate_modules_t callback, void *restrict arg)
{
    size_t i;
    const struct limine_file *file;

    if(module_request.response) {
        for(i = 0; i < module_request.response->module_count; i++) {
            file = module_request.response->modules[i];
            callback(file->path, file->address, file->size, arg);
        }

        return 0;
    }

    return EINVAL;
}

static void init_machine_boot(void)
{
    if(terminal_request.response) {
        // Register the console as soon as possible, so
        // if something fails after, it will be displayed regardless
        boot_console.cn_next = NULL;
        boot_console.cn_putchar = &boot_console_putchar;
        boot_console.cn_unblank = NULL;
        kstrncpy(boot_console.cn_name, "limine_terminal", sizeof(boot_console.cn_name));
        boot_console.cn_data = NULL;
        register_console(&boot_console);
    }

    // We need these to boot normally
    kassert(hhdm_request.response != NULL);
    kassert(memmap_request.response != NULL);
    kassert(kernel_address_request.response != NULL);

    hhdm_offset = hhdm_request.response->offset;
    kernel_base_phys = kernel_address_request.response->physical_base;
    kernel_base_virt = kernel_address_request.response->virtual_base;

}
early_initcall(machine_boot, init_machine_boot);
