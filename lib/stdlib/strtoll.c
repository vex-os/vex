/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <limits.h>
#include <stdlib.h>
#include <strtoi.h>

long long strtoll(const char *restrict s, const char **restrict endptr, int base)
{
    long long r;
    strtoi_t ctx = { 0 };
    ctx.base = base;
    ctx.signchar = '+';
    
    strtoi_init(s, &ctx);
    if(!ctx.buf) {
        if(endptr)
            *endptr = NULL;
        return 0;
    }

    if(ctx.signchar == '+') {
        ctx.inval = (uintmax_t)(LLONG_MAX);
        ctx.limit = (uintmax_t)(LLONG_MAX / ctx.base);
        ctx.limdigit = (int)(LLONG_MAX % ctx.base);
    }
    else {
        ctx.inval = (uintmax_t)(LLONG_MIN);
        ctx.limit = (uintmax_t)(LLONG_MIN / -ctx.base);
        ctx.limdigit = (int)(-(LLONG_MIN % ctx.base));
    }

    r = (long long)strtoi_run(&ctx);    
    if(endptr)
        *endptr = ctx.buf ? ctx.buf : s;
    return (ctx.signchar == '+') ? r : -r;
}
