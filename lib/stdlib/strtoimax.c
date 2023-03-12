/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <stdlib.h>
#include <strtoi.h>

intmax_t strtoimax(const char *restrict s, const char **restrict endptr, int base)
{
    intmax_t r;
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
        ctx.inval = (uintmax_t)(INTMAX_MAX);
        ctx.limit = (uintmax_t)(INTMAX_MAX / ctx.base);
        ctx.limdigit = (int)(INTMAX_MAX % ctx.base);
    }
    else {
        ctx.inval = (uintmax_t)(INTMAX_MIN);
        ctx.limit = (uintmax_t)(INTMAX_MIN / -ctx.base);
        ctx.limdigit = (int)(-(INTMAX_MIN % ctx.base));
    }

    r = (intmax_t)strtoi_run(&ctx);    
    if(endptr)
        *endptr = ctx.buf ? ctx.buf : s;
    return (ctx.signchar == '+') ? r : -r;
}
