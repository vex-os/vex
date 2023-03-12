/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strtoi.h>

#define MAX_BASE 36
static const char digits[MAX_BASE] = "0123456789abcdefghijklmnopqrstuvwxyz";

void strtoi_init(strtoi_t *restrict ctx, const char *restrict s)
{
    while(isspace(*s)) {
        s++;
    }
    
    ctx->signchar = ((*s != '+' && *s != '-') ? '+' : *s++);

    if(*s == '0') {
        s++;

        if((ctx->base == 0 || ctx->base == 2) && (*s == 'b' || *s == 'B')) {
            ctx->base = 2;
            if(!memchr(digits, tolower(*++s), ctx->base)) {
                s -= 2;
            }
        }
        else if((ctx->base == 0 || ctx->base == 16) && (*s == 'x' || *s == 'X')) {
            ctx->base = 16;
            if(!memchr(digits, tolower(*++s), ctx->base)) {
                s -= 2;
            }
        }
        else if(ctx->base == 0) {
            ctx->base = 0;
            s -= 1;
        }
        else {
            s -= 1;
        }
    }
    else if(ctx->base == 0) {
        ctx->base = 10;
    }

    ctx->buf = ((ctx->base >= 2 && ctx->base <= MAX_BASE) ? s : NULL);
}

uintmax_t strtoi_run(strtoi_t *restrict ctx)
{
    int digit = -1;
    uintmax_t r = 0;
    const char *x;

    while((x = memchr(digits, tolower(*ctx->buf), ctx->base))) {
        digit = x - digits;

        if(r < ctx->limit || (r == ctx->limit && digit < ctx->limdigit)) {
            r *= ctx->base;
            r += digit;
            ctx->buf++;
        }
        else {
            while(memchr(digits, tolower(*ctx->buf), ctx->base))
                ctx->buf++;
            ctx->signchar = '+';
            return ctx->inval;
        }
    }
    
    if(digit == -1) {
        ctx->buf = NULL;
        return 0;
    }

    return r;
}
