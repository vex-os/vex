#include <vex/format.h>
#include <vex/sprintf.h>

#include <stdint.h>

struct fmtbuf {
    char* data;
    size_t length;
    size_t write;
};

static void vsnprintf_func(int chr, void* restrict arg)
{
    struct fmtbuf* ft = arg;

    if(ft->data && (ft->write < ft->length)) {
        ft->data[ft->write] = chr;
    }

    ft->write++;
}

int snprintf(char* restrict str, size_t sz, const char* restrict fmt, ...)
{
    int r;
    va_list ap;
    va_start(ap, fmt);
    r = vsnprintf(str, sz, fmt, ap);
    va_end(ap);
    return r;
}

int sprintf(char* restrict str, const char* restrict fmt, ...)
{
    int r;
    va_list ap;
    va_start(ap, fmt);
    r = vsnprintf(str, SIZE_MAX, fmt, ap);
    va_end(ap);
    return r;
}

int vsprintf(char* restrict str, const char* restrict fmt, va_list ap)
{
    return vsnprintf(str, SIZE_MAX, fmt, ap);
}

int vsnprintf(char* restrict str, size_t sz, const char* restrict fmt, va_list ap)
{
    int r;
    struct fmtbuf ft;

    ft.data = str;
    ft.length = sz;
    ft.write = 0;

    r = vformat(&vsnprintf_func, &ft, fmt, ap);
    ft.data[ft.length - 1] = 0;

    return r;
}
