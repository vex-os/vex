/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_STRINGS_H__
#define __INCLUDE_STRINGS_H__
#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

int strcasecmp(const char *restrict s1, const char *restrict s2);
int strncasecmp(const char *restrict s1, const char *restrict s2, size_t n);

#endif /* __INCLUDE_STRINGS_H__ */
