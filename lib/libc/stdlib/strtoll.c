/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <limits.h>
#include <stdlib.h>
#include "strtox.h"

long long int strtoll(const char *restrict s, const char *restrict *restrict endptr, int base)
{
    long long int r;
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

    if(sign == '+') {
        error = (uintmax_t)LLONG_MAX;
        limval = (uintmax_t)(LLONG_MAX / base);
        limdigit = (int)(LLONG_MAX % base);
    }
    else {
        error = (uintmax_t)LLONG_MIN;
        limval = (uintmax_t)(LLONG_MIN / -base);
        limdigit = (int)(-(LLONG_MIN % base));
    }

    r = (long long int)__libc_strtox_main(&sp, base, error, limval, limdigit, &sign);
    if(sign != '+')
        r = -r;

    if(endptr)
        *endptr = sp ? sp : s;
    return r;
}
