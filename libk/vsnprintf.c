/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2023, VX/sys Contributors */
#include <pfmt.h>
#include <stdio.h>

struct fmtbuf {
    char *ft_data;
    size_t ft_length;
    size_t ft_write;
};

static void func(int c, void *restrict arg)
{
    struct fmtbuf *ft = arg;
    if(ft->ft_data && (ft->ft_write < ft->ft_length))
        ft->ft_data[ft->ft_write] = c;
    ft->ft_write++;
}

int vsnprintf(char *restrict s, size_t n, const char *restrict fmt, va_list ap)
{
    int r;
    struct fmtbuf ft;
    
    ft.ft_data = s;
    ft.ft_length = n;
    ft.ft_write = 0;

    r = vpfmt(&func, &ft, fmt, ap);
    ft.ft_data[ft.ft_length - 1] = 0;

    return r;
}
