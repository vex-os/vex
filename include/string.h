/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_STRING_H__
#define __INCLUDE_STRING_H__
#include <stddef.h>
#include <sys/cdefs.h>

const void *memchr(const void *restrict s, int c, size_t n);
int memcmp(const void *restrict s1, const void *restrict s2, size_t n);
void *memcpy(void *restrict s1, const void *restrict s2, size_t n);
void *memset(void *restrict s, int c, size_t n);

char *kstrncat(char *restrict s1, const char *restrict s2, size_t n);
char *kstrncpy(char *restrict s1, const char *restrict s2, size_t n);
char *strcat(char *restrict s1, const char *restrict s2);
char *strcpy(char *restrict s1, const char *restrict s2);
char *strerror(int errnum);
char *strncat(char *restrict s1, const char *restrict s2, size_t n);
char *strncpy(char *restrict s1, const char *restrict s2, size_t n);
const char *strchr(const char *restrict s, int c);
const char *strpbrk(const char *restrict s1, const char *restrict s2);
const char *strrchr(const char *restrict s, int c);
const char *strstr(const char *restrict s1, const char *restrict s2);
int strcmp(const char *restrict s1, const char *restrict s2);
int strerror_r(int errnum, char *restrict s, size_t n);
int strncmp(const char *restrict s1, const char *restrict s2, size_t n);
size_t strcspn(const char *restrict s1, const char *restrict s2);
size_t strlen(const char *restrict s);
size_t strnlen(const char *restrict s, size_t n);
size_t strspn(const char *restrict s1, const char *restrict s2);

#endif /* __INCLUDE_STRING_H__ */
