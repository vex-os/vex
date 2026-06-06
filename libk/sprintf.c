#include <format.h>
#include <sprintf.h>

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
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(str, sz, fmt, ap);
    va_end(ap);
    return r;
}

int sprintf(char* restrict str, const char* restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(str, SIZE_MAX, fmt, ap);
    va_end(ap);
    return r;
}

int vsprintf(char* restrict str, const char* restrict fmt, va_list ap)
{
    return vsnprintf(str, SIZE_MAX, fmt, ap);
}

int vsnprintf(char* restrict str, size_t sz, const char* restrict fmt, va_list ap)
{
    struct fmtbuf ft = { .data = str, .length = sz, .write = 0 };
    int r = vformat(&vsnprintf_func, &ft, fmt, ap);

    if(ft.data && ft.length > 0) {
        if(ft.write < ft.length) {
            ft.data[ft.write] = 0;
        }
        else {
            ft.data[ft.length - 1] = 0;
        }
    }

    return r;
}
