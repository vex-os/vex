#ifndef _VEX_STRINGS_H
#define _VEX_STRINGS_H 1

#include <vex/compiler.h>

#include <stddef.h>

extern char* kstrncat(char* restrict dst, const char* restrict src, size_t sz);
extern char* kstrncpy(char* restrict dst, const char* restrict src, size_t sz);
extern int strcasecmp(const char* restrict sa, const char* restrict sb) __nodiscard;
extern int strncasecmp(const char* restrict sa, const char* restrict sb, size_t sz) __nodiscard;

#endif
