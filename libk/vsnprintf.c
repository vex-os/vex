/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
/* UNDONE: floating point support if not __kernel__ */
#include <ctype.h>
#include <sprintf.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define FLAGS_ZEROPAD   0x01
#define FLAGS_LEFT      0x02
#define FLAGS_PLUS      0x04
#define FLAGS_SPACE     0x08
#define FLAGS_HASH      0x10
#define FLAGS_SIGNED    0x20
#define FLAGS_UPPERCASE 0x40

#define VALTYPE_CHAR    0x00
#define VALTYPE_SHORT   0x01
#define VALTYPE_INT     0x02
#define VALTYPE_LONG    0x03
#define VALTYPE_2LONG   0x04
#define VALTYPE_INTMAX  0x05
#define VALTYPE_SIZE    0x06
#define VALTYPE_PTRDIFF 0x07
#define VALTYPE_OTHER   0x08

typedef struct buf_s {
    char *buffer;
    size_t length;
    size_t writepos;
} buf_t;

static void buf_chr(buf_t *buf, int c)
{
    if(buf->buffer && buf->writepos < buf->length)
        buf->buffer[buf->writepos] = c;
    buf->writepos++;
}

#define MAX_BASE 16
static void buf_num(buf_t *buf, uintmax_t val, int neg, short flags, size_t w, size_t p, int base)
{
    size_t n;
    int pad, sign;
    const char *digits;
    char tempbuf_str[96];
    buf_t tempbuf;

    /* Clamp the base */
    if(base > MAX_BASE)
        base = MAX_BASE;

    /* Set padding character */
    pad = (flags & FLAGS_ZEROPAD) ? '0' : ' ';

    /* Set sign character */
    sign = 0;
    if(neg && (flags & FLAGS_SIGNED))
        sign = '-';
    else if(flags & FLAGS_SPACE)
        sign = ' ';
    else if(flags & FLAGS_PLUS)
        sign = '+';

    /* Set digits */
    if(flags & FLAGS_UPPERCASE) {
        digits = "0123456789ABCDEF";
    }
    else {
        digits = "0123456789abcdef";
    }

    /* Setup temporary buffer */
    memset(tempbuf_str, 0, sizeof(tempbuf_str));
    tempbuf.buffer = tempbuf_str;
    tempbuf.length = sizeof(tempbuf_str) - 1;
    tempbuf.writepos = 0;

    /* Write the value */
    do {
        buf_chr(&tempbuf, digits[val % base]);
        val /= base;
        if(p)
            p--;
    } while(val > 0 || p);

    /* Write the sign character */
    if(sign)
        buf_chr(&tempbuf, sign);

    /* Append a specific prefix for each base */
    if(flags & FLAGS_HASH) {
        if(!(flags & FLAGS_ZEROPAD)) {
            switch(base) {
                case 2:
                    buf_chr(&tempbuf, 'b');
                    buf_chr(&tempbuf, '0');
                    break;
                case 16:
                    buf_chr(&tempbuf, 'x');
                    buf_chr(&tempbuf, '0');
                    break;
            }
        }

        if(base == 8) {
            /* Octals start with a zero */
            buf_chr(&tempbuf, '0');
        }
    }

    /* Store the current position.
     * We allow overflow in buf_chr to
     * store the "imaginary" position here so we
     * can pad the resulting value with spaces later. */
    n = tempbuf.writepos;

    if(!(flags & FLAGS_LEFT)) {
        /* Pad with the pad character */
        while(tempbuf.writepos < w)
            buf_chr(&tempbuf, pad);

        /* Append a specific prefix for each base */
        if((flags & FLAGS_HASH) && (flags & FLAGS_ZEROPAD)) {
            switch(base) {
                case 2:
                    buf_chr(&tempbuf, 'b');
                    buf_chr(&tempbuf, '0');
                    break;
                case 16:
                    buf_chr(&tempbuf, 'x');
                    buf_chr(&tempbuf, '0');
                    break;
            }
        }
    }

    /* Reverse tempbuf to the output buffer */
    if(tempbuf.writepos >= tempbuf.length)
        tempbuf.writepos = tempbuf.length;
    while(tempbuf.writepos--)
        buf_chr(buf, tempbuf.buffer[tempbuf.writepos]);

    /* Pad with spaces */
    if(flags & FLAGS_LEFT) {
        while(n < w) {
            buf_chr(buf, ' ');
            n++;
        }
    }
}

static void buf_str(buf_t *buf, const char *s, short flags, size_t w, size_t p)
{
    size_t i, n;

    if(!s)
        s = "(null)";
    n = p ? strnlen(s, p) : strlen(s);

    /* Pad with spaces */
    if(!(flags & FLAGS_LEFT)) {
        for(i = n; i < w; i++)
            buf_chr(buf, ' ');
    }

    /* Copy the string */
    while(*s)
        buf_chr(buf, *s++);

    /* Pad with spaces */
    if(flags & FLAGS_LEFT) {
        for(i = n; i < w; i++)
            buf_chr(buf, ' ');
    }
}

static short get_flag(int ch)
{
    switch(ch) {
        case '0':
            return FLAGS_ZEROPAD;
        case '-':
            return FLAGS_LEFT;
        case '+':
            return FLAGS_PLUS;
        case ' ':
            return FLAGS_SPACE;
        case '#':
            return FLAGS_HASH;
        default:
            return 0;
    }
}

static uintmax_t remove_sign(uintmax_t val, int valtype, int *neg)
{
    #define __hack(type) if((type)val < 0) { *neg = 1; val = (uintmax_t)(-((type)val)); }
    switch(valtype) {
        case VALTYPE_CHAR:
            __hack(signed char);
            break;
        case VALTYPE_SHORT:
            __hack(signed short);
            break;
        case VALTYPE_INT:
            __hack(signed int);
            break;
        case VALTYPE_LONG:
            __hack(signed long);
            break;
        case VALTYPE_2LONG:
            __hack(signed long long);
            break;
        case VALTYPE_INTMAX:
            __hack(intmax_t);
            break;
        case VALTYPE_PTRDIFF:
            __hack(ptrdiff_t);
            break;
    }
    return val;
    #undef __hack
}

size_t vsnprintf(char *restrict s, size_t n, const char *restrict fmt, va_list va)
{
    buf_t buf;
    short flags, fval;
    const char *tmp;
    size_t w, p;
    uintmax_t val;
    int neg;
    int valtype;

    buf.buffer = s;
    buf.length = n - 1;
    buf.writepos = 0;

    while(*fmt) {
        if(*fmt != '%' || *++fmt == '%') {
            buf_chr(&buf, *fmt++);
            continue;
        }

        flags = 0;
        while((fval = get_flag(*fmt))) {
            flags |= fval;
            fmt++;
        }

        /* Filter invalid sequences */
        if((flags & FLAGS_ZEROPAD) && (flags & FLAGS_LEFT))
            flags &= ~FLAGS_ZEROPAD;

        w = strtouz(fmt, &tmp, 10);
        fmt = tmp;
        p = 0;

        if(*fmt == '.') {
            fmt++;
            p = strtouz(fmt, &tmp, 10);
            fmt = tmp;
        }

        valtype = VALTYPE_INT;
        switch(*fmt) {
            case 'h':
                fmt++;
                valtype = VALTYPE_SHORT;
                if(*fmt == 'h') {
                    fmt++;
                    valtype = VALTYPE_CHAR;
                }
                break;
            case 'l':
                fmt++;
                valtype = VALTYPE_LONG;
                if(*fmt == 'l') {
                    fmt++;
                    valtype = VALTYPE_2LONG;
                }
                break;
            case 'j':
                fmt++;
                valtype = VALTYPE_INTMAX;
                break;
            case 'z':
                fmt++;
                valtype = VALTYPE_SIZE;
                break;
            case 't':
                fmt++;
                valtype = VALTYPE_PTRDIFF;
                break;
        }

        if(*fmt == 's' || *fmt == 'p') {
            /* VALTYPE_OTHER tells the next switch
             * statement to not sample the variable
             * argument YET, we do this ourselves for
             * pointer arguments much later in the code. */
            valtype = VALTYPE_OTHER;
        }

        val = 0;
        switch(valtype) {
            case VALTYPE_CHAR:
            case VALTYPE_SHORT:
            case VALTYPE_INT:
                val = (uintmax_t)va_arg(va, unsigned int);
                break;
            case VALTYPE_LONG:
                val = (uintmax_t)va_arg(va, unsigned long);
                break;
            case VALTYPE_2LONG:
                val = (uintmax_t)va_arg(va, unsigned long long);
                break;
            case VALTYPE_INTMAX:
                val = (uintmax_t)va_arg(va, uintmax_t);
                break;
            case VALTYPE_SIZE:
                val = (uintmax_t)va_arg(va, size_t);
                break;
            case VALTYPE_PTRDIFF:
                val = (uintmax_t)va_arg(va, ptrdiff_t);
                break;
        }

        neg = 0;
        switch(*fmt) {
            case 'i':
            case 'd':
                flags |= FLAGS_SIGNED;
                val = remove_sign(val, valtype, &neg);
                buf_num(&buf, val, neg, flags, w, p, 10);
                break;
            case 'u':
                buf_num(&buf, val, neg, flags, w, p, 10);
                break;
            case 'x':
                buf_num(&buf, val, neg, flags, w, p, 16);
                break;
            case 'X':
                flags |= FLAGS_UPPERCASE;
                buf_num(&buf, val, neg, flags, w, p, 16);
                break;
            case 'o':
                buf_num(&buf, val, neg, flags, w, p, 8);
                break;
            case 'b':
                buf_num(&buf, val, neg, flags, w, p, 2);
                break;
            case 'c':
                buf_chr(&buf, (char)val);
                break;
            case 's':
                buf_str(&buf, va_arg(va, const char *), flags, w, p);
                break;
            case 'p':
                flags |= FLAGS_ZEROPAD;
                flags |= FLAGS_HASH;
                flags |= FLAGS_UPPERCASE;
                /* sizeof(uintptr_t) * 2: each byte defines two hex digits */
                buf_num(&buf, va_arg(va, uintmax_t), 0, flags, sizeof(uintptr_t) * 2, p, 16);
                break;
        }

        fmt++;
    }

    buf_chr(&buf, 0);
    if(buf.buffer)
        buf.buffer[buf.length - 1] = 0;
    return buf.writepos;
}
