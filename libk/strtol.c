/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2022, VX/sys Contributors */
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

long strtol(const char *restrict nptr, const char **restrict endptr, int base)
{
    int c;
    int minus;
    int limdigit;
    long limit;
    long accum;
    const char *cptr;
    const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

    do {
        c = *nptr++;
    } while(isspace(c));

    if(c == '-') {
        minus = 1;
        c = *nptr++;
    }
    else {
        if(c == '+')
            c = *nptr++;
        minus = 0;
    }

    if(c == '0') {
        if((base == 0 || base == 2) && (*nptr == 'B' || *nptr == 'b')) {
            base = 2;
            c = nptr[1];
            nptr += 2;
        }
        else if((base == 0 || base == 16) && (*nptr == 'X' || *nptr == 'x')) {
            base = 16;
            c = nptr[1];
            nptr += 2;
        }
        else if(base == 0) {
            base = 8;
        }
    }
    else if(base == 0) {
        base = 10;
    }

    base = ((base < 2) ? 2 : base);
    base = ((base > 36) ? 36 : base);
    accum = 0;

    if(minus) {
        limdigit = LONG_MIN % -base;
        limit = LONG_MIN / -base;
    }
    else {
        limdigit = LONG_MAX % base;
        limit = LONG_MAX / base;
    }

    while((cptr = memchr(digits, tolower(c), base)) != NULL) {
        c = cptr - digits;

        if(accum < limit || (accum == limit && c < limdigit)) {
            accum *= base;
            accum += c;
            c = *nptr++;
        }
        else {
            do {
                c = *++nptr;
            } while(memchr(digits, tolower(c), base));
            if(endptr)
                *endptr = nptr;
            return minus ? LONG_MIN : LONG_MAX;
        }
    }

    if(endptr)
        *endptr = nptr - 1;
    if(minus)
        return -accum;
    return accum;
}
