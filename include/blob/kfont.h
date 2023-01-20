/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef _INCLUDE_BLOB_KFONT_H__
#define _INCLUDE_BLOB_KFONT_H__
#include <kan/compiler.h>
#include <stdint.h>

extern const char __weak __kfont[];
extern const char __weak __kfont_end[];
extern const uint64_t __weak __kfont_size[1];

#endif /* _INCLUDE_BLOB_KFONT_H__ */
