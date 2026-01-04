#ifndef _VEX_STRING_H
#define _VEX_STRING_H 1

#include <vex/compiler.h>

#include <stddef.h>

extern const void* memchr(const void* restrict buf, int chr, size_t sz) __nodiscard;
extern int memcmp(const void* restrict ba, const void* restrict bb, size_t sz) __nodiscard;
extern void* memcpy(void* restrict dst, const void* restrict src, size_t sz);
extern void* memset(void* restrict dst, int chr, size_t sz);

extern char* strcat(char* restrict dst, const char* restrict src);
extern char* strcpy(char* restrict dst, const char* restrict src);
extern char* strerror(int errnum) __nodiscard;
extern char* strncat(char* restrict dst, const char* restrict src, size_t sz);
extern char* strncpy(char* restrict dst, const char* restrict src, size_t sz);
extern const char* strchr(const char* restrict str, int chr) __nodiscard;
extern const char* strpbrk(const char* restrict str, const char* restrict sym) __nodiscard;
extern const char* strrchr(const char* restrict str, int chr) __nodiscard;
extern const char* strstr(const char* restrict str, const char* restrict substr) __nodiscard;
extern int strcmp(const char* restrict sa, const char* restrict sb) __nodiscard;
extern int strerror_r(int errnum, char* restrict str, size_t sz);
extern int strncmp(const char* restrict sa, const char* restrict sb, size_t sz) __nodiscard;
extern size_t strcspn(const char* restrict str, const char* restrict sym) __nodiscard;
extern size_t strlen(const char* restrict str) __nodiscard;
extern size_t strnlen(const char* restrict str, size_t sz) __nodiscard;
extern size_t strspn(const char* restrict str, const char* restrict sym) __nodiscard;

#endif
