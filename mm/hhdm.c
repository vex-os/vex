/* SPDX-License-Identifier: Zlib */
#include <kern/panic.h>
#include <limine.h>
#include <mm/hhdm.h>
#include <stddef.h>

uintptr_t hhdm_offset;

static volatile struct limine_hhdm_request __used request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = NULL,
};

void init_hhdm(void)
{
    if(!request.response) {
        panic("hhdm: limine_hhdm_request has no response");
        unreachable();
    }

    hhdm_offset = request.response->offset;
}
