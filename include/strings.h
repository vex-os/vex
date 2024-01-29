/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2023, VX/sys Contributors */
#ifndef INCLUDE_STRINGS_H
#define INCLUDE_STRINGS_H
#include <stddef.h>
#include <vx/cdefs.h>

char *kstrncat(char *restrict s1, const char *restrict s2, size_t n);
char *kstrncpy(char *restrict s1, const char *restrict s2, size_t n);
int strcasecmp(const char *restrict s1, const char *restrict s2);
int strncasecmp(const char *restrict s1, const char *restrict s2, size_t n);

#endif /* INCLUDE_STRINGS_H */
