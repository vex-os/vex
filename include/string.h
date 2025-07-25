// SPDX-License-Identifier: BSD-2-Clause
#ifndef INCLUDE_STRING_H
#define INCLUDE_STRING_H
#include <kern/compiler.h>
#include <stddef.h>

const void* memchr(const void* restrict buf, int chr, size_t sz) __nodiscard;
int memcmp(const void* restrict ba, const void* restrict bb, size_t sz) __nodiscard;
void* memcpy(void* restrict dst, const void* restrict src, size_t sz);
void* memset(void* restrict dst, int chr, size_t sz);

char* strcat(char* restrict dst, const char* restrict src);
char* strcpy(char* restrict dst, const char* restrict src);
char* strerror(int errnum) __nodiscard;
char* strncat(char* restrict dst, const char* restrict src, size_t sz);
char* strncpy(char* restrict dst, const char* restrict src, size_t sz);
const char* strchr(const char* restrict str, int chr) __nodiscard;
const char* strpbrk(const char* restrict str, const char* restrict sym) __nodiscard;
const char* strrchr(const char* restrict str, int chr) __nodiscard;
const char* strstr(const char* restrict str, const char* restrict substr) __nodiscard;
int strcmp(const char* restrict sa, const char* restrict sb) __nodiscard;
int strerror_r(int errnum, char* restrict str, size_t sz);
int strncmp(const char* restrict sa, const char* restrict sb, size_t sz) __nodiscard;
size_t strcspn(const char* restrict str, const char* restrict sym) __nodiscard;
size_t strlen(const char* restrict str) __nodiscard;
size_t strnlen(const char* restrict str, size_t sz) __nodiscard;
size_t strspn(const char* restrict str, const char* restrict sym) __nodiscard;

#endif // INCLUDE_STRING_H
