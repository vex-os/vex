/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <limine.h>
#include <string.h>
#include <sys/boot.h>
#include <sys/debug.h>
#include <sys/pmalloc.h>

static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = NULL
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

static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0,
    .response = NULL,
};

static volatile struct limine_terminal_request terminal_request = {
    .id = LIMINE_TERMINAL_REQUEST,
    .revision = 0,
    .response = NULL,
    .callback = NULL,
};

uintptr_t hhdm_offset = 0;
uintptr_t load_address_phys = 0;
uintptr_t load_address_virt = 0;
console_t boot_console = { 0 };
const boot_memmap_t *boot_memmap = NULL;
const boot_module_t *boot_modules = NULL;

// Stored for future use
static size_t boot_memmap_npages = 0;
static size_t boot_modules_npages = 0;

// This works by shifting the address value of the first
// ever memory segment that loosely fits the desired object size.
static void *boot_malloc(size_t npages)
{
    size_t i;
    size_t n = npages * PAGE_SIZE;
    struct limine_memmap_entry *entry;
    void *result;

    for(i = 0; i < memmap_request.response->entry_count; i++) {
        entry = memmap_request.response->entries[i];

        if(entry->type == LIMINE_MEMMAP_USABLE && entry->length >= n) {
            result = (void *)(entry->base + hhdm_offset);
            entry->base += n;
            entry->length -= n;
            return result;
        }
    }

    return NULL;
}

static void boot_console_putchar(console_t *restrict console, int c)
{
    size_t i;
    char value = c;
    for(i = 0; i < terminal_request.response->terminal_count; i++) {
        terminal_request.response->write(terminal_request.response->terminals[i], &value, 1);
    }
}

static void init_boot(void)
{
    size_t i;
    unsigned short entry_type;
    boot_memmap_t *boot_memmap_writable;
    boot_module_t *boot_modules_writable;

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
    load_address_phys = kernel_address_request.response->physical_base;
    load_address_virt = kernel_address_request.response->virtual_base;

    boot_memmap_npages = get_page_count((memmap_request.response->entry_count + 1) * sizeof(boot_memmap_t));
    boot_memmap_writable = boot_malloc(boot_memmap_npages);
    kassertf(boot_memmap_writable, "boot: insufficient memory");

    for(i = 0; i < memmap_request.response->entry_count; i++) {
        switch(memmap_request.response->entries[i]->type) {
            case LIMINE_MEMMAP_KERNEL_AND_MODULES:
                entry_type = BOOT_MEMMAP_KERNEL;
                break;
            case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
                entry_type = BOOT_MEMMAP_LOADER;
                break;
            case LIMINE_MEMMAP_USABLE:
                entry_type = BOOT_MEMMAP_USABLE;
                break;
            default:
                entry_type = BOOT_MEMMAP_RESERV;
                break;
        }

        boot_memmap_writable[i].address = memmap_request.response->entries[i]->base;
        boot_memmap_writable[i].length = memmap_request.response->entries[i]->length;
        boot_memmap_writable[i].type = entry_type;
    }

    // The memmap table terminates with an entry of BOOT_MEMMAP_LAST type
    boot_memmap_writable[memmap_request.response->entry_count].address = 0;
    boot_memmap_writable[memmap_request.response->entry_count].length = 0;
    boot_memmap_writable[memmap_request.response->entry_count].type = BOOT_MEMMAP_LAST;

    if(module_request.response) {
        boot_modules_npages = get_page_count((module_request.response->module_count + 1) * sizeof(boot_module_t));
        boot_modules_writable = boot_malloc(boot_modules_npages);

        if(boot_modules_writable) {
            for(i = 0; i < module_request.response->module_count; i++) {
                boot_modules_writable[i].path = module_request.response->modules[i]->path;
                boot_modules_writable[i].data = module_request.response->modules[i]->address;
                boot_modules_writable[i].length = module_request.response->modules[i]->size;
            }

            // The module list terminates with a module which path and data are null
            boot_modules_writable[module_request.response->module_count].path = NULL;
            boot_modules_writable[module_request.response->module_count].data = NULL;
            boot_modules_writable[module_request.response->module_count].length = 0;
        }
        else {
            boot_modules_npages = 0;
        }
    }
    else {
        boot_modules_npages = 0;
        boot_modules_writable = NULL;
    }

    boot_memmap = boot_memmap_writable;
    boot_modules = boot_modules_writable;
}
early_initcall(boot, init_boot);
