/* SPDX-License-Identifier: GPL-2.0-only */
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

unsigned long strtoul(const char *restrict nptr, const char **restrict endptr, int base)
{
    int chr;
    int minus;
    int limdigit;
    unsigned long limit;
    unsigned long accum;
    const char *cptr;
    const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

    do {
        chr = *nptr++;
    } while(isspace(chr));

    if(chr == '-') {
        minus = 1;
        chr = *nptr++;
    }
    else {
        if(chr == '+')
            chr = *nptr++;
        minus = 0;
    }

    if(chr == '0') {
        if((base == 0 || base == 2) && (*nptr == 'B' || *nptr == 'b')) {
            base = 2;
            chr = nptr[1];
            nptr += 2;
        }
        else if((base == 0 || base == 16) && (*nptr == 'X' || *nptr == 'x')) {
            base = 16;
            chr = nptr[1];
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
    limdigit = ULONG_MAX % base;
    limit = ULONG_MAX / base;
    accum = 0;

    while((cptr = memchr(digits, tolower(chr), base)) != NULL) {
        chr = cptr - digits;

        if(accum < limit || (accum == limit && chr < limdigit)) {
            accum *= base;
            accum += chr;
            chr = *nptr++;
        }
        else {
            do {
                chr = *++nptr;
            } while(memchr(digits, tolower(chr), base));
            if(endptr)
                *endptr = nptr;
            return ULONG_MAX;
        }
    }

    if(endptr)
        *endptr = nptr - 1;
    if(minus)
        return -accum;
    return accum;
}
