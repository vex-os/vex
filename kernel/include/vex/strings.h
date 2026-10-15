#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

char* kstrncat(char* restrict dst, const char* restrict src, size_t sz);
char* kstrncpy(char* restrict dst, const char* restrict src, size_t sz);
int strcasecmp(const char* restrict sa, const char* restrict sb) __nodiscard;
int strncasecmp(const char* restrict sa, const char* restrict sb, size_t sz) __nodiscard;
