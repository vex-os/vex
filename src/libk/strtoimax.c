/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <stdlib.h>

intmax_t strtoimax(const char *restrict s, const char **restrict endptr, int base)
{
    intmax_t r;
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
        ctx.error = (uintmax_t)(INTMAX_MAX);
        ctx.limval = (uintmax_t)(INTMAX_MAX / ctx.base);
        ctx.limdigit = (int)(INTMAX_MAX % ctx.base);
    }
    else {
        ctx.error = (uintmax_t)(INTMAX_MIN);
        ctx.limval = (uintmax_t)(INTMAX_MIN / -ctx.base);
        ctx.limdigit = (int)(-(INTMAX_MIN % ctx.base));
    }

    r = (intmax_t)strtoxx_main(&ctx);
    if(ctx.sign != '+')
        r = -r;
    
    if(endptr)
        *endptr = ctx.s ? ctx.s : s;
    return r;
}
