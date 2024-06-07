/* SPDX-License-Identifier: GPL-2.0-only */
#include <format.h>
#include <string.h>
#include <sys/console.h>
#include <sys/printf.h>

#define LF 0x0A
#define CR 0x0D

#define STAGEBUF_SIZE 32

struct stagebuf {
    char st_data[STAGEBUF_SIZE];
    size_t st_size;
};

unsigned loglevel = KP_DEBUG;
size_t kmsg_writepos = 0;
char kmsg[KMSG_SIZE] = { 0 };

static void kmsg_format(int chr, void *restrict arg)
{
    struct stagebuf *stage = arg;
    stage->st_data[stage->st_size++] = chr;

    if(stage->st_size >= STAGEBUF_SIZE) {
        console_write_all(stage->st_data, STAGEBUF_SIZE);
        memset(stage->st_data, 0, STAGEBUF_SIZE);
        stage->st_size = 0;
    }

    kmsg[kmsg_writepos++] = chr;
    kmsg_writepos %= KMSG_SIZE;
}

void kputs(unsigned sv, const char *restrict str)
{
    size_t i;
    struct stagebuf st = { 0 };

    if(sv <= loglevel) {
        for(i = 0; str[i]; kmsg_format(str[i], &st));
        kmsg_format(CR, &st);
        kmsg_format(LF, &st);
    }

    if(st.st_size) {
        /* FIXME: should this be inside the
         * severity vs loglevel check above? */
        console_write_all(st.st_data, st.st_size);
    }
}

void kprintf(unsigned sv, const char *restrict fmt, ...)
{
    va_list ap;
    struct stagebuf st = { 0 };

    if(sv <= loglevel) {
        va_start(ap, fmt);
        vformat(&kmsg_format, &st, fmt, ap);
        kmsg_format(CR, &st);
        kmsg_format(LF, &st);
        va_end(ap);
    }

    if(st.st_size) {
        /* FIXME: should this be inside the
         * severity vs loglevel check above? */
        console_write_all(st.st_data, st.st_size);
    }
}

void kvprintf(unsigned sv, const char *restrict fmt, va_list ap)
{
    struct stagebuf st = { 0 };

    if(sv <= loglevel) {
        vformat(&kmsg_format, &st, fmt, ap);
        kmsg_format(CR, &st);
        kmsg_format(LF, &st);
    }

    if(st.st_size) {
        /* FIXME: should this be inside the
         * severity vs loglevel check above? */
        console_write_all(st.st_data, st.st_size);
    }
}
