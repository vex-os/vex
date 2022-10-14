/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <limits.h>
#include <stdlib.h>
#include "strtox.h"

unsigned long int strtoul(const char *restrict s, const char *restrict *restrict endptr, int base)
{
    unsigned long int r;
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

    error = (uintmax_t)ULONG_MAX;
    limval = (uintmax_t)(ULONG_MAX / base);
    limdigit = (int)(ULONG_MAX % base);

    /* FIXME: should we do the sign magic in the unsigned
     * realm as we are doing in the signed realm? */
    r = (unsigned long int)__libc_strtox_main(&sp, base, error, limval, limdigit, &sign);

    if(endptr)
        *endptr = sp ? sp : s;
    return r;
}
