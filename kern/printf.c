/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#include <kern/console.h>
#include <kern/printf.h>
#include <pfmt.h>
#include <string.h>

#define LF 0x0A
#define CR 0x0D

/* FIXME: BUFSIZE for libk and kernel code? */
#define STAGEBUF_SIZE 32

struct stagebuf {
    char st_data[STAGEBUF_SIZE];
    size_t st_size;
};

unsigned int loglevel = KP_DEBUG;
size_t kmsg_writepos = 0;
char kmsg[KMSG_SIZE] = { 0 };

static void kp_pfmt(int ch, void *restrict arg)
{
    struct stagebuf *stage = arg;
    stage->st_data[stage->st_size++] = ch;

    if(stage->st_size >= STAGEBUF_SIZE) {
        console_write(stage->st_data, STAGEBUF_SIZE);
        memset(stage->st_data, 0, STAGEBUF_SIZE);
        stage->st_size = 0;
    }

    kmsg[kmsg_writepos++] = ch;
    kmsg_writepos %= KMSG_SIZE;
}

void kputs(unsigned int sv, const char *restrict str)
{
    size_t i;
    struct stagebuf st = { 0 };

    if(sv <= loglevel) {
        for(i = 0; str[i]; kp_pfmt(str[i], &st));
        kp_pfmt(CR, &st);
        kp_pfmt(LF, &st);
    }

    if(st.st_size) {
        /* FIXME: should this be inside the
         * severity vs loglevel check above? */
        console_write(st.st_data, st.st_size);
    }
}

void kprintf(unsigned int sv, const char *restrict fmt, ...)
{
    va_list ap;
    struct stagebuf st = { 0 };

    if(sv <= loglevel) {
        va_start(ap, fmt);
        vpfmt(&kp_pfmt, &st, fmt, ap);
        kp_pfmt(CR, &st);
        kp_pfmt(LF, &st);
        va_end(ap);
    }

    if(st.st_size) {
        /* FIXME: should this be inside the
         * severity vs loglevel check above? */
        console_write(st.st_data, st.st_size);
    }
}

void kvprintf(unsigned int sv, const char *restrict fmt, va_list ap)
{
    struct stagebuf st = { 0 };

    if(sv <= loglevel) {
        vpfmt(&kp_pfmt, &st, fmt, ap);
        kp_pfmt(CR, &st);
        kp_pfmt(LF, &st);
    }

    if(st.st_size) {
        /* FIXME: should this be inside the
         * severity vs loglevel check above? */
        console_write(st.st_data, st.st_size);
    }
}
