/* SPDX-License-Identifier: BSD-2-Clause */
#include <string.h>

size_t strnlen(const char *restrict str, size_t sz)
{
    size_t i;
    for(i = 0; *str++ && i < sz; i++);
    return i;
}
