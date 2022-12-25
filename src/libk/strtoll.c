/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <limits.h>
#include <stdlib.h>

long long strtoll(const char *restrict s, const char **restrict endptr, int base)
{
    long long r;
    strtoxx_ctx_t ctx = { 0 };
    ctx.base = base;
    ctx.sign = '+';
    
    strtoxx_init(s, &ctx);
    if(!ctx.s) {
        if(endptr)
            *endptr = NULL;
        return 0;
    }

    if(ctx.sign == '+') {
        ctx.error = (uintmax_t)(LLONG_MAX);
        ctx.limval = (uintmax_t)(LLONG_MAX / ctx.base);
        ctx.limdigit = (int)(LLONG_MAX % ctx.base);
    }
    else {
        ctx.error = (uintmax_t)(LLONG_MIN);
        ctx.limval = (uintmax_t)(LLONG_MIN / -ctx.base);
        ctx.limdigit = (int)(-(LLONG_MIN % ctx.base));
    }

    r = (long long)strtoxx_main(&ctx);
    if(ctx.sign != '+')
        r = -r;
    
    if(endptr)
        *endptr = ctx.s ? ctx.s : s;
    return r;
}
