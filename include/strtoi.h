/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_STRTOI_H__
#define __INCLUDE_STRTOI_H__
#include <stdint.h>
#include <sys/cdefs.h>

typedef struct strtoi_s {
    const char *s_buf;
    uintmax_t s_inval;
    uintmax_t s_limit;
    int s_limdigit;
    int s_signchar;
    int s_base;
} strtoi_t;

void strtoi_init(const char *restrict s, strtoi_t *restrict ctx);
uintmax_t strtoi_run(strtoi_t *restrict ctx);

#endif /* __INCLUDE_STRTOI_H__ */
