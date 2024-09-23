/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef INCLUDE_MM_MEMMAP_H
#define INCLUDE_MM_MEMMAP_H
#include <kern/compiler.h>
#include <limine.h>

extern volatile struct limine_memmap_request memmap;

void init_memmap(void);

#endif /* INCLUDE_MM_MEMMAP_H */
