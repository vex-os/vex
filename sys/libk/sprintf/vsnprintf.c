/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2018, Joe Davis <me@jo.ie> */
/* Copyright (C) 2020, 2021, 2022, Kaneru contributors */
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <sprintf.h>

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

struct charbuf_s {
    char *buffer;
    size_t length;
    size_t writepos;
};

static void writechar_charbuf(struct charbuf_s *charbuf, int c)
{
    if(charbuf->buffer && charbuf->writepos < charbuf->length)
        charbuf->buffer[charbuf->writepos] = c;
    charbuf->writepos++;
}

#define MAX_BASE 16
static void writenum_charbuf(struct charbuf_s *charbuf, uintmax_t value, int negative, unsigned int flags, size_t width, size_t precision, unsigned int base)
{
    size_t n;
    int pad, sign;
    const char *digits;
    char tempbuf_str[96];
    struct charbuf_s tempbuf;

    /* Clamp the base */
    if(base > MAX_BASE)
        base = MAX_BASE;

    /* Set padding character */
    pad = (flags & FLAGS_ZEROPAD) ? '0' : ' ';

    /* Set sign character */
    sign = 0;
    if(negative && (flags & FLAGS_SIGNED))
        sign = '-';
    else if(flags & FLAGS_SPACE)
        sign = ' ';
    else if(flags & FLAGS_PLUS)
        sign = '+';

    /* Set digits */
    if(flags & FLAGS_UPPERCASE)
        digits = "0123456789ABCDEF";
    else
        digits = "0123456789abcdef";

    /* Setup temporary buffer */
    memset(tempbuf_str, 0, sizeof(tempbuf_str));
    tempbuf.buffer = tempbuf_str;
    tempbuf.length = sizeof(tempbuf_str) - 1;
    tempbuf.writepos = 0;

    /* Write the value */
    do {
        writechar_charbuf(&tempbuf, digits[value % base]);
        value /= base;
        if(precision)
            precision--;
    } while(value > 0 || precision);

    /* Write the sign character */
    if(sign)
        writechar_charbuf(&tempbuf, sign);

    /* Append a specific prefix for each base */
    if(flags & FLAGS_HASH) {
        if(!(flags & FLAGS_ZEROPAD)) {
            switch(base) {
                case 2:
                    writechar_charbuf(&tempbuf, 'b');
                    writechar_charbuf(&tempbuf, '0');
                    break;
                case 16:
                    writechar_charbuf(&tempbuf, 'x');
                    writechar_charbuf(&tempbuf, '0');
                    break;
            }
        }

        if(base == 8) {
            /* Octals start with a zero */
            writechar_charbuf(&tempbuf, '0');
        }
    }

    /* Store the current position.
     * We allow overflow in writechar_charbuf to
     * store the "imaginary" position here so we
     * can pad the resulting value with spaces later. */
    n = tempbuf.writepos;

    if(!(flags & FLAGS_LEFT)) {
        /* Pad with the pad character */
        while(tempbuf.writepos < width)
            writechar_charbuf(&tempbuf, pad);

        /* Append a specific prefix for each base */
        if((flags & FLAGS_HASH) && (flags & FLAGS_ZEROPAD)) {
            switch(base) {
                case 2:
                    writechar_charbuf(&tempbuf, 'b');
                    writechar_charbuf(&tempbuf, '0');
                    break;
                case 16:
                    writechar_charbuf(&tempbuf, 'x');
                    writechar_charbuf(&tempbuf, '0');
                    break;
            }
        }
    }

    /* Reverse tempbuf to the output buffer */
    if(tempbuf.writepos >= tempbuf.length)
        tempbuf.writepos = tempbuf.length;
    while(tempbuf.writepos--)
        writechar_charbuf(charbuf, tempbuf.buffer[tempbuf.writepos]);

    /* Pad with spaces */
    if(flags & FLAGS_LEFT) {
        while(n < width) {
            writechar_charbuf(charbuf, ' ');
            n++;
        }
    }
}

static void writestr_charbuf(struct charbuf_s *charbuf, const char *s, unsigned int flags, size_t width, size_t precision)
{
    size_t i, n;
    n = precision ? strnlen(s, precision) : strlen(s);

    /* Pad with spaces */
    if(!(flags & FLAGS_LEFT)) {
        for(i = n; i < width; i++)
            writechar_charbuf(charbuf, ' ');
    }

    /* Copy the string */
    while(*s)
        writechar_charbuf(charbuf, *s++);

    /* Pad with spaces */
    if(flags & FLAGS_LEFT) {
        for(i = n; i < width; i++)
            writechar_charbuf(charbuf, ' ');
    }
}

static unsigned int get_flag(int ch)
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

static size_t strtosize(const char *s, const char *restrict *out)
{
    size_t value = 0;
    *out = s;
    while(isdigit(**out)) {
        value = (value * 10) + (**out - '0');
        (*out)++;
    }

    return value;
}

static uintmax_t remove_sign(uintmax_t value, int valtype, int *negative)
{
    #define __hack(type) if((type)value < 0) { *negative = 1; value = (uintmax_t)(-((type)value)); }
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
    return value;
    #undef __hack
}

int vsnprintf(char *restrict s, size_t n, const char *restrict fmt, va_list va)
{
    struct charbuf_s charbuf;
    unsigned int flags, fval;
    size_t width, precision;
    uintmax_t value;
    int negative;
    int valtype;

    charbuf.buffer = s;
    charbuf.length = n - 1;
    charbuf.writepos = 0;

    while(*fmt) {
        if(*fmt != '%' || *++fmt == '%') {
            writechar_charbuf(&charbuf, *fmt++);
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

        width = strtosize(fmt, &fmt);
        precision = 0;

        if(*fmt == '.') {
            fmt++;
            precision = strtosize(fmt, &fmt);
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

        value = 0;
        switch(valtype) {
            case VALTYPE_CHAR:
            case VALTYPE_SHORT:
            case VALTYPE_INT:
                value = (uintmax_t)va_arg(va, unsigned int);
                break;
            case VALTYPE_LONG:
                value = (uintmax_t)va_arg(va, unsigned long);
                break;
            case VALTYPE_2LONG:
                value = (uintmax_t)va_arg(va, unsigned long long);
                break;
            case VALTYPE_INTMAX:
                value = (uintmax_t)va_arg(va, uintmax_t);
                break;
            case VALTYPE_SIZE:
                value = (uintmax_t)va_arg(va, size_t);
                break;
            case VALTYPE_PTRDIFF:
                value = (uintmax_t)va_arg(va, ptrdiff_t);
                break;
        }

        negative = 0;
        switch(*fmt) {
            case 'i':
            case 'd':
                flags |= FLAGS_SIGNED;
                value = remove_sign(value, valtype, &negative);
                writenum_charbuf(&charbuf, value, negative, flags, width, precision, 10);
                break;
            case 'u':
                writenum_charbuf(&charbuf, value, negative, flags, width, precision, 10);
                break;
            case 'x':
                writenum_charbuf(&charbuf, value, negative, flags, width, precision, 16);
                break;
            case 'X':
                flags |= FLAGS_UPPERCASE;
                writenum_charbuf(&charbuf, value, negative, flags, width, precision, 16);
                break;
            case 'o':
                writenum_charbuf(&charbuf, value, negative, flags, width, precision, 8);
                break;
            case 'b':
                writenum_charbuf(&charbuf, value, negative, flags, width, precision, 2);
                break;
            case 'c':
                writechar_charbuf(&charbuf, (char)value);
                break;
            case 's':
                writestr_charbuf(&charbuf, va_arg(va, const char *), flags, width, precision);
                break;
            case 'p':
                flags |= FLAGS_ZEROPAD;
                flags |= FLAGS_HASH;
                flags |= FLAGS_UPPERCASE;
                /* sizeof(uintptr_t) * 2: each byte defines two hex digits */
                writenum_charbuf(&charbuf, (uintmax_t)va_arg(va, const void *), 0, flags, sizeof(uintptr_t) * 2, precision, 16);
                break;
        }

        fmt++;
    }

    writechar_charbuf(&charbuf, 0);
    if(charbuf.buffer)
        charbuf.buffer[charbuf.length - 1] = 0;
    return (int)charbuf.writepos;
}
