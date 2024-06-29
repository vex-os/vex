// SPDX-License-Identifier: Zlib
#ifndef INCLUDE_MM_KBASE_H
#define INCLUDE_MM_KBASE_H
#include <kern/compiler.h>
#include <stdint.h>

extern uintptr_t kbase_phys;
extern uintptr_t kbase_virt;

void init_kbase(void);

#endif // INCLUDE_MM_KBASE_H
