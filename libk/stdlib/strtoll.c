// SPDX-License-Identifier: BSD-2-Clause
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

long long strtoll(const char* restrict nptr, const char** restrict endptr, int base)
{
    int chr;
    int minus;
    int limdigit;
    long long limit;
    long long accum;
    const char* cptr;
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
    accum = 0;

    if(minus) {
        limdigit = LLONG_MIN % -base;
        limit = LLONG_MIN / -base;
    }
    else {
        limdigit = LLONG_MAX % base;
        limit = LLONG_MAX / base;
    }

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
            return minus ? LLONG_MIN : LLONG_MAX;
        }
    }

    if(endptr)
        *endptr = nptr - 1;
    if(minus)
        return -accum;
    return accum;
}
