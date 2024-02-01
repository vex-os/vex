/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#include <kern/boot.h>
#include <kern/initcall.h>
#include <kern/panic.h>
#include <kern/printf.h>
#include <kern/version.h>
#include <stddef.h>

uintptr_t hhdm_offset = 0;
uintptr_t kernel_base_phys = 0;
uintptr_t kernel_base_virt = 0;

volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = NULL,
};

volatile struct limine_kernel_address_request kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0,
    .response = NULL,
};

volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
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
    kernel_base_phys = kernel_address_request.response->physical_base;
    kernel_base_virt = kernel_address_request.response->virtual_base;

    /* UNDONE: should SMP and sched use initcalls? */
    for(i = 0; initcalls[i]; initcalls[i++]());

    panic("main: nothing else to do!");
    UNREACHABLE();
}
