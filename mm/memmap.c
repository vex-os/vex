/* SPDX-License-Identifier: Zlib */
#include <kern/panic.h>
#include <mm/memmap.h>
#include <stddef.h>

volatile struct limine_memmap_request __used memmap = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
    .response = NULL,
};

void init_memmap(void)
{
    if(!memmap.response) {
        panic("memmap: limine_memmap_request has no response");
        unreachable();
    }
}
