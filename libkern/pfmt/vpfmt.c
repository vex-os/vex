/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <limits.h>
#include <pfmt.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define F_ZERO_PAD  0x0001 /* 0         */
#define F_LEFT      0x0002 /* -         */
#define F_PLUS      0x0004 /* +         */
#define F_SPACE     0x0008 /* space     */
#define F_HASH      0x0010 /* #         */
#define F_SIGNED    0x0020 /* special   */
#define F_UPPERCASE 0x0040 /* special   */
#define F_NEGATIVE  0x0080 /* special   */

#define L_CHAR      0x0000 /* hh        */
#define L_SHORT     0x0001 /* h         */
#define L_INT       0x0002 /* nothing   */
#define L_LONG      0x0003 /* l         */
#define L_LLONG     0x0004 /* ll        */
#define L_INTMAX_T  0x0005 /* j         */
#define L_SIZE_T    0x0006 /* z         */
#define L_PTRDIFF_T 0x0007 /* t         */
#define L_POINTER   0x0007 /* special   */

#define MAX_BASE 16
static const char lc_digits[MAX_BASE + 1] = "0123456789abcdef";
static const char uc_digits[MAX_BASE + 1] = "0123456789ABCDEF";

static size_t write_int(pfmt_func_t func, void *restrict arg, uintmax_t value, unsigned short flags, unsigned int w, unsigned int p, unsigned int base)
{
    size_t stager;
    size_t counter;
    int sign, pad;
    unsigned int i;
    const char *digits;
    char stage[128] = { 0 };

    if((flags & F_NEGATIVE) && (flags & F_SIGNED)) {
        /* The number is both signed and considered
         * to be less than zero, we have to print a sign. */
        sign = '-';
    }
    else if(flags & F_PLUS) {
        /* The number is either signed and positive
         * or unsigned; and we were requested to print a sign. */
        sign = '+';
    }
    else if(flags & F_SPACE) {
        /* If a number is signed (and it is), we want a
         * whitespace placed in the place a minus would be. */
        sign = ' ';
    }
    else sign = 0x00;

    if(!(flags & F_LEFT) && (flags & F_ZERO_PAD)) {
        /* Normally numeric values are padded (ie. moved)
         * with a whitespace character but if we are not
         * left-justified, we can use leading zeros for that. */
        pad = '0';
    }
    else pad = ' ';

    stager = 0;
    counter = 0;
    digits = ((flags & F_UPPERCASE) ? uc_digits : lc_digits);

    do {
        stage[stager++] = digits[value % base];
        if(p > 0)
            p--;
        value /= base;
    } while(value || p);

    if(sign) {
        /* Add the sign character */
        stage[stager++] = sign;
    }

    if(flags & F_HASH) {
        if(!(flags & F_ZERO_PAD)) {
            switch(base) {
                case 2:
                    stage[stager++] = 'b';
                    stage[stager++] = '0';
                    break;
                case 16:
                    stage[stager++] = 'x';
                    stage[stager++] = '0';
                    break;
            }
        }
        else if(base == 8) {
            /* Octals start with a zero so we can
             * safely assume we won't miss when adding
             * the numeric prefix right here. */
            stage[stager++] = '0';
        }
    }

    if(!(flags & F_LEFT)) {
        /* Add numeric prefix in cases of zero-padding
         * as the prefixes don't exist in the stage buffer */
        if((flags & F_HASH) && (flags & F_ZERO_PAD)) {
            switch(base) {
                case 2:
                    func('0', arg);
                    func('b', arg);
                    counter += 2;
                    break;
                case 16:
                    func('0', arg);
                    func('x', arg);
                    counter += 2;
                    break;
            }
        }

        for(i = stager; i < w; i++) {
            func(pad, arg);
            counter++;
        }
    }

    /* Reverse the number into the callback */
    while(stager--) {
        func(stage[stager], arg);
        counter++;
    }

    /* And now pad the number */
    if(flags & F_LEFT) {
        for(i = 0; i < w; i++) {
            func(' ', arg);
            counter++;
        }
    }

    return counter;
}

static const char defined_behaviour[] = "(null)";
static size_t write_str(pfmt_func_t func, void *restrict arg, const char *restrict s, unsigned short flags, unsigned int w, unsigned int p)
{
    size_t i;
    size_t cap;
    size_t len;
    size_t counter = 0;

    if(s == NULL) {
        /* C standard does not define what happens when you try
         * dereferencing a NULL char pointer. Because NO ONE at INCITS/C
         * expects you to pass a NULL pointer to snprintf. Too bad! */
        s = defined_behaviour;
    }

    if(p) {
        cap = p;
        len = strnlen(s, p);
    }
    else {
        cap = SIZE_MAX;
        len = strlen(s);
    }

    if(!(flags & F_LEFT)) {
        for(i = len; cap && i < w; i++) {
            func(' ', arg);
            counter++;
            cap--;
        }
    }

    while(cap && *s) {
        func(*s++, arg);
        counter++;
        cap--;
    }

    if(flags & F_LEFT) {
        for(i = len; cap && i < w; i++) {
            func(' ', arg);
            counter++;
            cap--;
        }
    }

    return counter;
}

static unsigned short get_flag(int c)
{
    switch(c) {
        case '0':
            return F_ZERO_PAD;
        case '-':
            return F_LEFT;
        case '+':
            return F_PLUS;
        case ' ':
            return F_SPACE;
        case '#':
            return F_HASH;
        default:
            return 0;
    }
}

static uintmax_t remove_sign(uintmax_t x, int length, unsigned short *restrict flags)
{
    #define sign_hack(type) if((type)x < 0) { *flags |= F_NEGATIVE; x = (uintmax_t)(-((type)x)); }
    switch(length) {
        case L_CHAR:
            sign_hack(signed char);
            break;
        case L_SHORT:
            sign_hack(signed short);
            break;
        case L_INT:
            sign_hack(signed int);
            break;
        case L_LONG:
            sign_hack(signed long);
            break;
        case L_LLONG:
            sign_hack(signed long long);
            break;
        case L_INTMAX_T:
            sign_hack(intmax_t);
            break;
        case L_PTRDIFF_T:
            sign_hack(ptrdiff_t);
            break;
    }
    return x;
    #undef sign_hack
}

int vpfmt(pfmt_func_t func, void *restrict arg, const char *restrict fmt, va_list ap)
{
    size_t counter = 0;
    unsigned int w, p;
    unsigned short flags;
    unsigned short fvalue;
    unsigned short length;
    const char *endptr;
    uintmax_t value;

    while(*fmt) {
        if(*fmt != '%' || *(++fmt) == '%') {
            func(*fmt++, arg);
            counter++;
            continue;
        }

        flags = 0;
        while((fvalue = get_flag(*fmt)) != 0) {
            flags |= fvalue;
            fmt++;
        }

        if((flags & F_ZERO_PAD) && (flags & F_LEFT)) {
            /* Cannot add zeros after the value as they
             * will not be leading zeros anymore and will
             * have a meaning, multiplying the number by 10 */
            flags &= ~F_ZERO_PAD;
        }

        w = strtoul(fmt, &endptr, 10);
        fmt = endptr;
        p = 0;

        if(*fmt == '.') {
            p = strtoul(++fmt, &endptr, 10);
            fmt = endptr;
        }

        length = L_INT;
        switch(*fmt) {
            case 'h':
                fmt++;
                length = L_SHORT;
                break;
            case 'l':
                fmt++;
                length = L_LONG;
                break;
            case 'j':
                fmt++;
                length = L_INTMAX_T;
                break;
            case 'z':
                fmt++;
                length = L_SIZE_T;
                break;
            case 't':
                fmt++;
                length = L_PTRDIFF_T;
                break;
        }

        if(length == L_SHORT || length == L_LONG) {
            switch(*fmt) {
                case 'h':
                    fmt++;
                    length = L_CHAR;
                    break;
                case 'l':
                    fmt++;
                    length = L_LLONG;
                    break;
            }
        }

        value = 0;
        if(*fmt == 's' || *fmt == 'p') {
            /* L_POINTER is a special case, it tells the next
             * switch statement that we don't want to call va_arg()
             * ahead of time, instead waiting for later code to do that. */
            length = L_POINTER;
        }
        else {
            switch(length) {
                case L_CHAR:
                case L_SHORT:
                case L_INT:
                    value = va_arg(ap, unsigned int);
                    break;
                case L_LONG:
                    value = va_arg(ap, unsigned long);
                    break;
                case L_LLONG:
                    value = va_arg(ap, unsigned long long);
                    break;
                case L_INTMAX_T:
                    value = va_arg(ap, uintmax_t);
                    break;
                case L_SIZE_T:
                    value = va_arg(ap, size_t);
                    break;
                case L_PTRDIFF_T:
                    /* That's right, we are assigning a signed ptrdiff_t to
                     * an unsigned uintmax_t variable. Because no one would
                     * ever try to fuck around with %ut and find out... Fuck! */
                    value = va_arg(ap, ptrdiff_t);
                    break;
            }
        }

        switch(*fmt) {
            case 'd':
            case 'i':
                flags |= F_SIGNED;
                value = remove_sign(value, length, &flags);
                counter += write_int(func, arg, value, flags, w, p, 10);
                break;
            case 'u':
                counter += write_int(func, arg, value, flags, w, p, 10);
                break;
            case 'x':
                counter += write_int(func, arg, value, flags, w, p, 16);
                break;
            case 'X':
                flags |= F_UPPERCASE;
                counter += write_int(func, arg, value, flags, w, p, 16);
                break;
            case 'o':
                counter += write_int(func, arg, value, flags, w, p, 8);
                break;
            case 'b':
                counter += write_int(func, arg, value, flags, w, p, 2);
                break;
            case 'c':
                func(value, arg);
                counter++;
                break;
            case 's':
                counter += write_str(func, arg, va_arg(ap, const char *), flags, w, p);
                break;
            case 'p':
                flags |= F_HASH;
                flags |= F_ZERO_PAD;
                flags |= F_UPPERCASE;
                counter += write_int(func, arg, va_arg(ap, uintptr_t), flags, sizeof(uintptr_t) * 2, p, 16);
                break;
            default:
                func('%', arg);
                func(*fmt, arg);
                counter += 2;
                break;
        }

        fmt++;
    }

    func(0, arg);

    if(counter > INT_MAX)
        return INT_MAX;
    return counter;
}
