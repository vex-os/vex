/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <stdlib.h>
#include "strtox.h"

size_t strtouz(const char *restrict s, const char *restrict *restrict endptr, int base)
{
    size_t r;
    int sign = '+';
    const char *sp = __libc_strtox_prelim(s, &sign, &base);
    uintmax_t error, limval;
    int limdigit;

    /* __libc_strtox_prelim returns NULL 
     * whenever the base is out of range */
    if(!sp) {
        if(endptr)
            *endptr = s;
        return 0;
    }

    error = (uintmax_t)SIZE_MAX;
    limval = (uintmax_t)(SIZE_MAX / base);
    limdigit = (int)(SIZE_MAX % base);

    /* FIXME: should we do the sign magic in the unsigned
     * realm as we are doing in the signed realm? */
    r = (size_t)__libc_strtox_main(&sp, base, error, limval, limdigit, &sign);

    if(endptr)
        *endptr = sp ? sp : s;
    return r;
}
