/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#ifndef INCLUDE_SYS_LINKER_H
#define INCLUDE_SYS_LINKER_H

extern const char start_text[];
extern const char start_rodata[];
extern const char start_data[];
extern const char start_bss[];

extern const char end_text[];
extern const char end_rodata[];
extern const char end_data[];
extern const char end_bss[];

#endif /* INCLUDE_SYS_LINKER_H */
