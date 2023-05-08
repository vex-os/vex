/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __X86_64_PAGING_H__
#define __X86_64_PAGING_H__
#include <stdint.h>

#define VMM_L5_SHIFT 48
#define VMM_L4_SHIFT 39
#define VMM_L3_SHIFT 30
#define VMM_L2_SHIFT 21
#define VMM_L1_SHIFT 12
#define VMM_PML_MASK 0x1FF

#define VMM_MODE_V 0x0000000000000001
#define VMM_MODE_R 0x0000000000000000
#define VMM_MODE_W 0x0000000000000002
#define VMM_MODE_X 0x8000000000000000
#define VMM_MODE_U 0x0000000000000004
#define VMM_MODE_G 0x0000000000000100
#define VMM_MODE_A 0x0000000000000020

#define VMM_MASK_A 0x000FFFFFFFFFF000
#define VMM_MASK_M 0xFFF0000000000FFF

typedef uint64_t pml_t;

#endif /* __X86_64_PAGING_H__ */
