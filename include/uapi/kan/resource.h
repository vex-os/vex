/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_UAPI_KAN_RESOURCE_H__
#define _INCLUDE_UAPI_KAN_RESOURCE_H__
#include <stddef.h>
#include <stdint.h>

#define RESOURCE_MMIO       (1 << 0)
#define RESOURCE_MMIO_A_16  (1 << 1)
#define RESOURCE_MMIO_A_32  (1 << 2)
#define RESOURCE_PMIO       (1 << 3)
#define RESOURCE_PMIO_SLOW  (1 << 4)

typedef struct resource_s {
    char name[64];
    size_t size;
    uintptr_t base;
    unsigned int flags;
    struct resource_s *next;
} resource_t;

#endif /* _INCLUDE_UAPI_KAN_RESOURCE_H__ */
