#include <vex/format.h>
#include <vex/sprintf.h>

typedef struct fmtbuf_s fmtbuf_t;

struct fmtbuf_s {
    char* data;
    size_t length;
    size_t write;
};

static void vsnprintf_func(int chr, void* restrict arg)
{
    fmtbuf_t* ft = arg;

    if(ft->data && (ft->write < ft->length)) {
        ft->data[ft->write] = chr;
    }

    ft->write += 1;
}

int snprintf(char* restrict str, size_t size, const char* restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(str, size, fmt, ap);
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

int vsnprintf(char* restrict str, size_t size, const char* restrict fmt, va_list ap)
{
    fmtbuf_t ft;
    ft.data = str;
    ft.length = size;
    ft.write = 0;

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
