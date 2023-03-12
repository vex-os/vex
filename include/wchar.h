/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef _SYS_INCLUDE_WCHAR_H__
#define _SYS_INCLUDE_WCHAR_H__
#include <stddef.h>
#include <sys/cdefs.h>

wchar_t *kwcsncat(wchar_t *restrict s1, const wchar_t *restrict s2, size_t n);
wchar_t *kwcsncpy(wchar_t *restrict s1, const wchar_t *restrict s2, size_t n);
wchar_t *wcscat(wchar_t *restrict s1, const wchar_t *restrict s2);
wchar_t *wcscpy(wchar_t *restrict s1, const wchar_t *restrict s2);
wchar_t *wcsncat(wchar_t *restrict s1, const wchar_t *restrict s2, size_t n);
wchar_t *wcsncpy(wchar_t *restrict s1, const wchar_t *restrict s2, size_t n);
const wchar_t *wcschr(const wchar_t *restrict s, wchar_t c);
const wchar_t *wcspbrk(const wchar_t *restrict s1, const wchar_t *restrict s2);
const wchar_t *wcsrchr(const wchar_t *restrict s, wchar_t c);
const wchar_t *wcswcs(const wchar_t *restrict s1, const wchar_t *restrict s2);
int wcscmp(const wchar_t *restrict s1, const wchar_t *restrict s2);
int wcsncmp(const wchar_t *restrict s1, const wchar_t *restrict s2, size_t n);
size_t wcscspn(const wchar_t *restrict s1, const wchar_t *restrict s2);
size_t wcslen(const wchar_t *restrict s);
size_t wcsnlen(const wchar_t *restrict s, size_t n);
size_t wcsspn(const wchar_t *restrict s1, const wchar_t *restrict s2);

#endif /* _SYS_INCLUDE_WCHAR_H__ */
