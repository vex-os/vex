#include <vex/consdrv.h>
#include <vex/format.h>
#include <vex/kprintf.h>
#include <vex/string.h>

#ifndef CONF_KPRINTF_DEFAULT_LEVEL
#define CONF_KPRINTF_DEFAULT_LEVEL KP_DEBUG
#endif

#define LF 0x0A
#define CR 0x0D

#define STAGEBUF_SIZE 32

struct stagebuf {
    char st_data[STAGEBUF_SIZE];
    size_t st_size;
};

size_t kprintf_writepos = 0U;
char kprintf_buffer[CONF_KPRINTF_BUFSIZE] = { 0 };
unsigned int loglevel = CONF_KPRINTF_DEFAULT_LEVEL;

static void kprintf_format_func(int chr, void* restrict arg)
{
    struct stagebuf* stage = arg;
    stage->st_data[stage->st_size++] = chr;

    if(stage->st_size >= STAGEBUF_SIZE) {
        consdrv_write(stage->st_data, STAGEBUF_SIZE);
        memset(stage->st_data, 0, STAGEBUF_SIZE);
        stage->st_size = 0;
    }

    kprintf_buffer[kprintf_writepos++] = chr;
    kprintf_writepos %= CONF_KPRINTF_BUFSIZE;
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

    if(st.st_size) {
        // FIXME: should this be inside the
        // severity vs loglevel check above?
        consdrv_write(st.st_data, st.st_size);
    }
}
