// SPDX-License-Identifier: Zlib
#include <string.h>

size_t strlen(const char *restrict str)
{
    size_t i;
    for(i = 0; *str++; i++);
    return i;
}
