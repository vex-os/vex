/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_X86_64_SEGMENT_H__
#define __INCLUDE_X86_64_SEGMENT_H__
#include <sys/initcall.h>

#define SEGMENT_NULL 0
#define SEGMENT_KERN_CODE_16 1
#define SEGMENT_KERN_DATA_16 2
#define SEGMENT_KERN_CODE_32 3
#define SEGMENT_KERN_DATA_32 4
#define SEGMENT_KERN_CODE_64 5
#define SEGMENT_KERN_DATA_64 6
#define SEGMENT_USER_CODE_64 7
#define SEGMENT_USER_DATA_64 8
#define SEGMENT_SELECTOR(index, ldt, ring) (((ring) & 4) | (((ldt) & 1) << 2) | (index) << 3)

initcall_extern(segment);

#endif /* __INCLUDE_X86_64_SEGMENT_H__ */
