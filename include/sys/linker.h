/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_LINKER_H__
#define __INCLUDE_SYS_LINKER_H__

extern const char start_text[];
extern const char start_rodata[];
extern const char start_data[];
extern const char start_bss[];

extern const char end_text[];
extern const char end_rodata[];
extern const char end_data[];
extern const char end_bss[];

#endif /* __INCLUDE_SYS_LINKER_H__ */
