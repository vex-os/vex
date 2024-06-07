/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _INCLUDE_STRINGS_H
#define _INCLUDE_STRINGS_H
#include <stddef.h>
#include <sys/cdefs.h>

char *kstrncat(char *restrict dst, const char *restrict src, size_t sz);
char *kstrncpy(char *restrict dst, const char *restrict src, size_t sz);
int strcasecmp(const char *restrict sa, const char *restrict sb) __nodiscard;
int strncasecmp(const char *restrict sa, const char *restrict sb, size_t sz) __nodiscard;

#endif /* _INCLUDE_STRINGS_H */
