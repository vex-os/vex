/* SPDX-License-Identifier: GPL-2.0-only */
#include <stddef.h>
#include <sys/boot.h>
#include <sys/initcall.h>
#include <sys/panic.h>
#include <sys/printf.h>
#include <sys/version.h>

uintptr_t hhdm_offset = 0x0000000000000000;
uintptr_t kernel_address_phys = 0x0000000000000000;
uintptr_t kernel_address_virt = 0x0000000000000000;

volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = NULL,
};

volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
    .response = NULL,
};

volatile struct limine_kernel_address_request kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0,
    .response = NULL,
};

volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
    .response = NULL,
};

void __noreturn __used main(void)
{
    size_t i;

    kprintf(KP_INFORM, "%s %s %s", sysname, release, version);

    if(!hhdm_request.response) {
        panic("main: hhdm_request has no response");
        UNREACHABLE();
    }

    if(!memmap_request.response) {
        panic("main: memmap_request has no response");
        UNREACHABLE();
    }

    if(!kernel_address_request.response) {
        panic("main: kernel_address_request has no response");
        UNREACHABLE();
    }

    hhdm_offset = hhdm_request.response->offset;
    kernel_address_phys = kernel_address_request.response->physical_base;
    kernel_address_virt = kernel_address_request.response->virtual_base;

    for(i = 0; initcalls[i]; initcalls[i++]());

    panic("main: nothing else to do");
    UNREACHABLE();
}
