/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2022, KanOS Contributors */
#include <ctype.h>
#include <inttypes.h>
#include <string.h>

uintmax_t strtoumax(const char *restrict nptr, const char **restrict endptr, int base)
{
    int c;
    int minus;
    int limdigit;
    uintmax_t limit;
    uintmax_t accum;
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
    limdigit = UINTMAX_MAX % base;
    limit = UINTMAX_MAX / base;
    accum = 0;

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
            return UINTMAX_MAX;
        }
    }

    if(endptr)
        *endptr = nptr - 1;
    if(minus)
        return -accum;
    return accum;
}
