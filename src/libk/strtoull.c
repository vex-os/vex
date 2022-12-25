/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <limits.h>
#include <stdlib.h>

unsigned long long strtoull(const char *restrict s, const char **restrict endptr, int base)
{
    unsigned long long r;
    strtoxx_ctx_t ctx = { 0 };
    ctx.base = base;
    ctx.sign = '+';
    
    strtoxx_init(s, &ctx);
    if(!ctx.s) {
        if(endptr)
            *endptr = NULL;
        return 0;
    }

    ctx.error = (uintmax_t)(ULLONG_MAX);
    ctx.limval = (uintmax_t)(ULLONG_MAX / ctx.base);
    ctx.limdigit = (int)(ULLONG_MAX % ctx.base);

    r = (unsigned long long)strtoxx_main(&ctx);
    if(ctx.sign != '+')
        r = -r;
    
    if(endptr)
        *endptr = ctx.s ? ctx.s : s;
    return r;
}
