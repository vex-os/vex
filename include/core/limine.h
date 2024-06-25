/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_CORE_LIMINE_H
#define INCLUDE_CORE_LIMINE_H
#include <limine.h>
#include <stdint.h>

extern uintptr_t hhdm_offset;
extern uintptr_t kernel_base_phys;
extern uintptr_t kernel_base_virt;

extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_kernel_address_request kernel_address_request;
extern volatile struct limine_paging_mode_request paging_mode_request;
extern volatile struct limine_framebuffer_request framebuffer_request;
extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_module_request module_request;

#endif /* INCLUDE_CORE_LIMINE_H */
