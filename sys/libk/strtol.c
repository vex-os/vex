/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <limits.h>
#include <stdlib.h>
#include "strtox.h"

long int strtol(const char *restrict s, const char *restrict *restrict endptr, int base)
{
    long int r;
    int sign = '+';
    const char *sp = _strtox_prelim(s, &sign, &base);
    uintmax_t error, limval;
    int limdigit;

    if(!sp) {
        if(endptr)
            *endptr = s;
        return 0;
    }

    if(sign == '+') {
        error = (uintmax_t)LONG_MAX;
        limval = (uintmax_t)(LONG_MAX / base);
        limdigit = (int)(LONG_MAX % base);
    }
    else {
        error = (uintmax_t)LONG_MIN;
        limval = (uintmax_t)(LONG_MIN / -base);
        limdigit = (int)(-(LONG_MIN % base));
    }

    r = (long int)_strtox_main(&sp, base, error, limval, limdigit, &sign);
    if(sign != '+')
        r = -r;

    if(endptr)
        *endptr = sp ? sp : s;
    return r;
}
