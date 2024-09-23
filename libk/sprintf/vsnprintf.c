/* SPDX-License-Identifier: BSD-2-Clause */
#include <format.h>
#include <sprintf.h>

struct fmtbuf {
    char *ft_data;
    size_t ft_length;
    size_t ft_write;
};

static void func(int chr, void *restrict arg)
{
    struct fmtbuf *ft = arg;
    if(ft->ft_data && (ft->ft_write < ft->ft_length))
        ft->ft_data[ft->ft_write] = chr;
    ft->ft_write++;
}

int vsnprintf(char *restrict str, size_t sz, const char *restrict fmt, va_list ap)
{
    int r;
    struct fmtbuf ft;
    
    ft.ft_data = str;
    ft.ft_length = sz;
    ft.ft_write = 0;

    r = vformat(&func, &ft, fmt, ap);
    ft.ft_data[ft.ft_length - 1] = 0;

    return r;
}
