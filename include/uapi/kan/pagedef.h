/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_UAPI_KAN_PAGEDEF_H__
#define _INCLUDE_UAPI_KAN_PAGEDEF_H__
#include <kan/compiler.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__X86_64__)
#include <x86/pagedef.h>
#define PAGE_SIZE X86_PAGE_SIZE
#else
#define PAGE_SIZE 4096
#endif

/* Returns page-aligned address */
static __force_inline uintptr_t page_align_address(uintptr_t address)
{
    return __align_floor(address, PAGE_SIZE);
}

/* Converts byte count to page count */
static __force_inline size_t get_page_count(size_t n)
{
    return __align_ceil(n, PAGE_SIZE) / PAGE_SIZE;
}

#endif /* _INCLUDE_UAPI_KAN_PAGE_H__ */
