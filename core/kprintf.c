#include <core/kprintf.h>
#include <dev/console.h>
#include <lib/format.h>
#include <lib/string.h>

#ifndef KP_DEFAULT_LOGLEVEL
#define KP_DEFAULT_LOGLEVEL KP_DEBUG
#endif

#define LF 0x0A
#define CR 0x0D

#define STAGEBUF_SIZE 32

struct stagebuf {
    char data[STAGEBUF_SIZE];
    size_t size;
};

size_t kp_writepos = 0;
char kp_buffer[KP_BUFSIZE];
unsigned int loglevel = KP_DEFAULT_LOGLEVEL;

static void kprintf_format_func(int chr, void* restrict arg)
{
    struct stagebuf* stage = arg;
    stage->data[stage->size++] = chr;

    if(stage->size >= STAGEBUF_SIZE) {
        console_write(stage->data, STAGEBUF_SIZE);
        memset(stage->data, 0, STAGEBUF_SIZE);
        stage->size = 0;
    }

    kp_buffer[kp_writepos++] = chr;
    kp_writepos %= KP_BUFSIZE;
}

void kprintf(unsigned int sv, const char* restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kvprintf(sv, fmt, ap);
    va_end(ap);
}

void kvprintf(unsigned int sv, const char* restrict fmt, va_list ap)
{
    struct stagebuf st = { 0 };

    if(sv <= loglevel) {
        vformat(&kprintf_format_func, &st, fmt, ap);
        kprintf_format_func(CR, &st);
        kprintf_format_func(LF, &st);
    }

    if(st.size) {
        console_write(st.data, st.size);
    }
}
