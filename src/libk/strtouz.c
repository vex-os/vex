/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <stdlib.h>

size_t strtouz(const char *restrict s, const char **restrict endptr, int base)
{
    size_t r;
    strtoxx_ctx_t ctx = { 0 };
    ctx.base = base;
    ctx.sign = '+';
    
    strtoxx_init(s, &ctx);
    if(!ctx.s) {
        if(endptr)
            *endptr = NULL;
        return 0;
    }

    ctx.error = (uintmax_t)(SIZE_MAX);
    ctx.limval = (uintmax_t)(SIZE_MAX / ctx.base);
    ctx.limdigit = (int)(SIZE_MAX % ctx.base);

    r = (size_t)strtoxx_main(&ctx);
    if(ctx.sign != '+')
        r = -r;
    
    if(endptr)
        *endptr = ctx.s ? ctx.s : s;
    return r;
}
