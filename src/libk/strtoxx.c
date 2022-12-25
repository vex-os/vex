/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BASE 36
static const char digits[MAX_BASE] = "0123456789abcdefghijklmnopqrstuvwxyz";

void strtoxx_init(const char *restrict s, strtoxx_ctx_t *restrict ctx)
{
    while(isspace(*s))
        s++;
    
    ctx->sign = ((*s != '+' && *s != '-') ? '+' : *s++);

    if(*s == '0') {
        s++;

        if((ctx->base == 0 || ctx->base == 2) && (*s == 'b' || *s == 'B')) {
            ctx->base = 2;
            if(!memchr(digits, tolower(*++s), ctx->base))
                s -= 2;
        }
        else if((ctx->base == 0 || ctx->base == 16) && (*s == 'x' || *s == 'X')) {
            ctx->base = 16;
            if(!memchr(digits, tolower(*++s), ctx->base))
                s -= 2;
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

    ctx->s = ((ctx->base >= 2 && ctx->base <= MAX_BASE) ? s : NULL);
}

uintmax_t strtoxx_main(strtoxx_ctx_t *restrict ctx)
{
    int digit = -1;
    uintmax_t r = 0;
    const char *x;

    while((x = memchr(digits, tolower(*ctx->s), ctx->base))) {
        digit = x - digits;

        if(r < ctx->limval || (r == ctx->limval && digit < ctx->limdigit)) {
            r *= ctx->base;
            r += digit;
            ctx->s++;
        }
        else {
            while(memchr(digits, tolower(*ctx->s), ctx->base))
                ctx->s++;
            ctx->sign = '+';
            return ctx->error;
        }
    }
    
    if(digit == -1) {
        ctx->s = NULL;
        return 0;
    }

    return r;
}
