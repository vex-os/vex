/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef _INCLUDE_KAN_PMEM_H__
#define _INCLUDE_KAN_PMEM_H__
#include <kan/compiler.h>
#include <kan/initcall.h>
#include <kan/pagedef.h>
#include <stddef.h>
#include <stdint.h>

uintptr_t pmalloc(size_t psize);
void *pmalloc_hhdm(size_t psize);
void pmfree(uintptr_t pptr, size_t psize);
void pmfree_hhdm(void *restrict ptr, size_t psize);

initcall_extern(pmem);

#endif /* _INCLUDE_KAN_PMEM_H__ */
