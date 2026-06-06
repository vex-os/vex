#ifndef BEBA1DCE_0EF3_45A9_99D0_F1BE21A28D88
#define BEBA1DCE_0EF3_45A9_99D0_F1BE21A28D88
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>

char* kstrncat(char* restrict dst, const char* restrict src, size_t sz);
char* kstrncpy(char* restrict dst, const char* restrict src, size_t sz);
int strcasecmp(const char* restrict sa, const char* restrict sb) __nodiscard;
int strncasecmp(const char* restrict sa, const char* restrict sb, size_t sz) __nodiscard;

#endif /* BEBA1DCE_0EF3_45A9_99D0_F1BE21A28D88 */
