/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strtoi.h>

#define MAX_BASE 36
static const char digits[MAX_BASE] = "0123456789abcdefghijklmnopqrstuvwxyz";

void strtoi_init(const char *restrict s, strtoi_t *restrict ctx)
{
    while(isspace(*s)) {
        s++;
    }
    
    ctx->s_signchar = ((*s != '+' && *s != '-') ? '+' : *s++);

    if(*s == '0') {
        s++;

        if((ctx->s_base == 0 || ctx->s_base == 2) && (*s == 'b' || *s == 'B')) {
            ctx->s_base = 2;
            if(!memchr(digits, tolower(*++s), ctx->s_base)) {
                s -= 2;
            }
        }
        else if((ctx->s_base == 0 || ctx->s_base == 16) && (*s == 'x' || *s == 'X')) {
            ctx->s_base = 16;
            if(!memchr(digits, tolower(*++s), ctx->s_base)) {
                s -= 2;
            }
        }
        else if(ctx->s_base == 0) {
            ctx->s_base = 0;
            s -= 1;
        }
        else {
            s -= 1;
        }
    }
    else if(ctx->s_base == 0) {
        ctx->s_base = 10;
    }

    ctx->s_buf = ((ctx->s_base >= 2 && ctx->s_base <= MAX_BASE) ? s : NULL);
}

uintmax_t strtoi_run(strtoi_t *restrict ctx)
{
    int digit = -1;
    uintmax_t r = 0;
    const char *x;

    while((x = memchr(digits, tolower(*ctx->s_buf), ctx->s_base))) {
        digit = x - digits;

        if(r < ctx->s_limit || (r == ctx->s_limit && digit < ctx->s_limdigit)) {
            r *= ctx->s_base;
            r += digit;
            ctx->s_buf++;
        }
        else {
            while(memchr(digits, tolower(*ctx->s_buf), ctx->s_base))
                ctx->s_buf++;
            ctx->s_signchar = '+';
            return ctx->s_inval;
        }
    }
    
    if(digit == -1) {
        ctx->s_buf = NULL;
        return 0;
    }

    return r;
}
