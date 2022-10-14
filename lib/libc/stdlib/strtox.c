/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <ctype.h>
#include <string.h>
#include "strtox.h"

const char __libc_strtox_digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

const char *__libc_strtox_prelim(const char *restrict s, int *sign, int *base)
{
    while(isspace(*s))
        s++;

    *sign = (*s != '+' && *s != '-') ? '+' : *s++;    

    if(*s == '0') {
        s++;

        if((*base == 0 || *base == 2) && (*s == 'b' || *s == 'B')) {
            *base = 2;
            if(!memchr(__libc_strtox_digits, tolower(*++s), *base))
                s -= 2;
        }
        else if((*base == 0 || *base == 16) && (*s == 'x' || *s == 'X')) {
            *base = 16;
            if(!memchr(__libc_strtox_digits, tolower(*++s), *base))
                s -= 2;
        }
        else if(*base == 0) {
            *base = 8;
            s--;
        }
        else {
            s--;
        }
    }
    else if(*base == 0) {
        *base = 10;
    }

    return (*base >= 2 && *base <= 36) ? s : NULL;
}

uintmax_t __libc_strtox_main(const char *restrict *restrict s, int base, uintmax_t error, uintmax_t limval, int limdigit, int *sign)
{
    int digit = -1;
    uintmax_t r = 0;
    const char *x;

    while((x = memchr(__libc_strtox_digits, tolower(**s), base))) {
        digit = x - __libc_strtox_digits;

        if(r < limval || (r == limval && digit <= limdigit)) {
            r *= base;
            r += digit;
            (*s)++;
        }
        else {
            while(memchr(__libc_strtox_digits, tolower(**s), base))
                (*s)++;
            *sign = '+';
            return error;
        }
    }

    if(digit == -1) {
        *s = NULL;
        return 0;
    }

    return r;
}
