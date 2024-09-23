/* SPDX-License-Identifier: Zlib */
#include <kern/panic.h>
#include <limine.h>
#include <mm/kbase.h>
#include <stddef.h>

uintptr_t kbase_phys;
uintptr_t kbase_virt;

static volatile struct limine_kernel_address_request __used request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0,
    .response = NULL,
};

void init_kbase(void)
{
    if(!request.response) {
        panic("kbase: limine_kernel_address_request has no response");
        unreachable();
    }

    kbase_phys = request.response->physical_base;
    kbase_virt = request.response->virtual_base;
}
