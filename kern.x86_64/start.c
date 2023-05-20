/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <iecprefix.h>
#include <machine/boot.h>
#include <printf.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/init.h>
#include <sys/klog.h>
#include <sys/panic.h>
#include <sys/pmm.h>
#include <sys/version.h>

#if LATER
volatile struct limine_5_level_paging_request five_level_paging_request = {
    .id = LIMINE_5_LEVEL_PAGING_REQUEST,
    .revision = 0,
    .response = NULL,
};
#endif

volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
    .response = NULL,
};

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

uintptr_t hhdm_offset = 0;
uintptr_t kernel_base_phys = 0;
uintptr_t kernel_base_virt = 0;
fb_info_t framebuffers[MAX_FRAMEBUFFERS] = { 0 };

#define ZONE_DMA_END MiB(16)
#define ZONE_DMA32_END GiB(4)

static void parse_memmap(const struct limine_memmap_entry *restrict entry)
{
    size_t split_length;
    struct limine_memmap_entry dummy = { 0 };

    dummy.base = entry->base;
    dummy.length = entry->length;

    if(dummy.base < ZONE_DMA_END) {
        if(dummy.base + dummy.length > ZONE_DMA_END) {
            split_length = ZONE_DMA_END - dummy.base;
            pmm_add_memblock(dummy.base, get_page_count(split_length), PMM_ZONE_NORMAL_BIT | PMM_ZONE_DMA_BIT | PMM_ZONE_DMA32_BIT);
            dummy.base += split_length;
            dummy.length -= split_length;
        }
        else {
            pmm_add_memblock(dummy.base, get_page_count(dummy.length), PMM_ZONE_NORMAL_BIT | PMM_ZONE_DMA_BIT | PMM_ZONE_DMA32_BIT);
            return;
        }
    }

    if(dummy.base < ZONE_DMA32_END) {
        if(dummy.base + dummy.length > ZONE_DMA32_END) {
            split_length = ZONE_DMA32_END - dummy.base;
            pmm_add_memblock(dummy.base, get_page_count(split_length), PMM_ZONE_NORMAL_BIT | PMM_ZONE_DMA32_BIT);
            dummy.base += split_length;
            dummy.length -= split_length;
        }
        else {
            pmm_add_memblock(dummy.base, get_page_count(dummy.length), PMM_ZONE_NORMAL_BIT | PMM_ZONE_DMA32_BIT);
            return;
        }
    }

    pmm_add_memblock(dummy.base, get_page_count(dummy.length), PMM_ZONE_NORMAL_BIT);
}

void __used __noreturn kstart(void)
{
    size_t i;

    klog(LOG_INFO, "kstart: starting version %s+%s", version, git_revision);

    kassertf(hhdm_request.response, "kstart: hhdm not available");
    kassertf(memmap_request.response, "kstart: memmap not available");
    kassertf(kernel_address_request.response, "kstart: kernel address not available");

    hhdm_offset = hhdm_request.response->offset;
    kernel_base_phys = kernel_address_request.response->physical_base;
    kernel_base_virt = kernel_address_request.response->virtual_base;
    memset(framebuffers, 0, sizeof(framebuffers));

    if(framebuffer_request.response) {
        for(i = 0; i < MAX_FRAMEBUFFERS && i < framebuffer_request.response->framebuffer_count; ++i) {
            snprintf(framebuffers[i].ident, sizeof(framebuffers[i].ident), "Limine FB%zu", i);
            framebuffers[i].base_offset = ((uint64_t)framebuffer_request.response->framebuffers[i]->address - hhdm_offset);
            framebuffers[i].mmio_offset = UINT64_C(0x0000000000000000);
            framebuffers[i].mmio_length = UINT64_C(0x0000000000000000);
            framebuffers[i].vid_width = framebuffer_request.response->framebuffers[i]->width;
            framebuffers[i].vid_height = framebuffer_request.response->framebuffers[i]->height;
            framebuffers[i].vid_scanline = framebuffer_request.response->framebuffers[i]->pitch;
            framebuffers[i].vid_depth = framebuffer_request.response->framebuffers[i]->bpp;
            framebuffers[i].vid_red.bits = ((1 << framebuffer_request.response->framebuffers[i]->red_mask_size) - 1);
            framebuffers[i].vid_green.bits = ((1 << framebuffer_request.response->framebuffers[i]->green_mask_size) - 1);
            framebuffers[i].vid_blue.bits = ((1 << framebuffer_request.response->framebuffers[i]->blue_mask_size) - 1);
            framebuffers[i].vid_red.offset = framebuffer_request.response->framebuffers[i]->red_mask_shift;
            framebuffers[i].vid_green.offset = framebuffer_request.response->framebuffers[i]->green_mask_shift;
            framebuffers[i].vid_blue.offset = framebuffer_request.response->framebuffers[i]->blue_mask_shift;
        }
    }

    for(i = 0; i < memmap_request.response->entry_count; ++i) {
        if(memmap_request.response->entries[i]->type != LIMINE_MEMMAP_USABLE)
            continue;
        parse_memmap(memmap_request.response->entries[i]);
    }

    /* UNDONE: should SMP and sched use initcalls? */
    for(i = 0; initcalls[i]; initcalls[i++]());

    panic("kstart: nothing else to do!");
    UNREACHABLE();
}
