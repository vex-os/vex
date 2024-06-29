// SPDX-License-Identifier: Zlib
#include <kern/panic.h>
#include <kern/printf.h>
#include <limine.h>
#include <mm/kbase.h>

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
        panic("kbase: bootloader response not present");
        unreachable();
    }

    kbase_phys = request.response->physical_base;
    kbase_virt = request.response->virtual_base;

    kprintf(KP_DEBUG, "kbase: phys=%p", (void *)kbase_phys);
    kprintf(KP_DEBUG, "kbase: virt=%p", (void *)kbase_virt);
}
