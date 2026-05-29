#ifndef BEBA1DCE_0EF3_45A9_99D0_F1BE21A28D88
#define BEBA1DCE_0EF3_45A9_99D0_F1BE21A28D88
#pragma once

#include <core/cdefs.h>
#include <stddef.h>

extern char* kstrncat(char* restrict dst, const char* restrict src, size_t sz);
extern char* kstrncpy(char* restrict dst, const char* restrict src, size_t sz);
extern int strcasecmp(const char* restrict sa, const char* restrict sb) __nodiscard;
extern int strncasecmp(const char* restrict sa, const char* restrict sb, size_t sz) __nodiscard;

#endif /* BEBA1DCE_0EF3_45A9_99D0_F1BE21A28D88 */
