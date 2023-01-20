/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef _INCLUDE_WCHAR_H__
#define _INCLUDE_WCHAR_H__
#include <kan/compiler.h>
#include <stddef.h>
#include <stdint.h>

uint32_t *kwcsncat(uint32_t *restrict s1, const uint32_t *restrict s2, size_t n);
uint32_t *kwcsncpy(uint32_t *restrict s1, const uint32_t *restrict s2, size_t n);
uint32_t *wcscat(uint32_t *restrict s1, const uint32_t *restrict s2);
uint32_t *wcscpy(uint32_t *restrict s1, const uint32_t *restrict s2);
uint32_t *wcsncat(uint32_t *restrict s1, const uint32_t *restrict s2, size_t n);
uint32_t *wcsncpy(uint32_t *restrict s1, const uint32_t *restrict s2, size_t n);
const uint32_t *wcschr(const uint32_t *restrict s, uint32_t c);
const uint32_t *wcspbrk(const uint32_t *restrict s1, const uint32_t *restrict s2);
const uint32_t *wcsrchr(const uint32_t *restrict s, uint32_t c);
const uint32_t *wcsstr(const uint32_t *restrict s1, const uint32_t *restrict s2);
int wcscmp(const uint32_t *restrict s1, const uint32_t *restrict s2);
int wcsncmp(const uint32_t *restrict s1, const uint32_t *restrict s2, size_t n);
size_t wcscspn(const uint32_t *restrict s1, const uint32_t *restrict s2);
size_t wcslen(const uint32_t *restrict s);
size_t wcsnlen(const uint32_t *restrict s, size_t n);
size_t wcsspn(const uint32_t *restrict s1, const uint32_t *restrict s2);

#endif /* _INCLUDE_WCHAR_H__ */
