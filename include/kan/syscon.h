/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_KAN_SYSCON_H__
#define _INCLUDE_KAN_SYSCON_H__
#include <uapi/kan/syscon.h>

int register_syscon(syscon_t *restrict con);
void unregister_syscon(syscon_t *restrict con);
void syscon_write(const void *restrict s, size_t n);

#endif /* _INCLUDE_KAN_SYSCON_H__ */
