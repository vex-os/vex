/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <limits.h>
#include <stdlib.h>

long strtol(const char *restrict s, const char **restrict endptr, int base)
{
    long r;
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
        ctx.error = (uintmax_t)(LONG_MAX);
        ctx.limval = (uintmax_t)(LONG_MAX / ctx.base);
        ctx.limdigit = (int)(LONG_MAX % ctx.base);
    }
    else {
        ctx.error = (uintmax_t)(LONG_MIN);
        ctx.limval = (uintmax_t)(LONG_MIN / -ctx.base);
        ctx.limdigit = (int)(-(LONG_MIN % ctx.base));
    }

    r = strtoxx_main(&ctx);
    if(ctx.sign != '+')
        r = -r;
    
    if(endptr)
        *endptr = ctx.s ? ctx.s : s;
    return r;
}
