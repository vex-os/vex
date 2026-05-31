#include <core/assert.h>
#include <core/cmdline.h>
#include <core/console.h>
#include <core/kprintf.h>
#include <core/spinlock.h>
#include <init/initcall.h>
#include <lib/format.h>
#include <lib/string.h>
#include <lib/strings.h>
#include <time/clocksrc.h>

#ifndef KP_DEFAULT_LOGLEVEL
#define KP_DEFAULT_LOGLEVEL KP_INFORM
#endif

#define LF 0x0A
#define CR 0x0D

#define STAGEBUF_SIZE 32

struct stagebuf {
    char data[STAGEBUF_SIZE];
    size_t size;
    int last;
};

size_t kp_writepos = 0;
char kp_buffer[KP_BUFSIZE];
unsigned loglevel = KP_DEFAULT_LOGLEVEL;

static spinlock_type kprintf_lock = SPINLOCK_INIT;

static void kprintf_format_func(int chr, void* restrict arg)
{
    struct stagebuf* stage = arg;
    stage->last = chr;
    stage->data[stage->size++] = chr;

    if(stage->size >= STAGEBUF_SIZE) {
        console_write(stage->data, STAGEBUF_SIZE);
        memset(stage->data, 0, STAGEBUF_SIZE);
        stage->size = 0;
    }

    kp_buffer[kp_writepos++] = chr;
    kp_writepos %= KP_BUFSIZE;
}

void kprintf(unsigned sv, const char* restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kvprintf(sv, fmt, ap);
    va_end(ap);
}

void kvprintf(unsigned sv, const char* restrict fmt, va_list ap)
{
    struct stagebuf st = { 0 };
    uint64_t timestamp = 0;
    uint64_t timestamp_s = 0;
    uint64_t timestamp_msx = 0;

    spinlock_acquire(&kprintf_lock);

    if(sv <= loglevel) {
        timestamp = clocksrc_read_nsec();
        timestamp_s = timestamp / UINT64_C(1000000000);
        timestamp_msx = (timestamp / UINT64_C(100000)) % UINT64_C(10000);

        format(&kprintf_format_func, &st, "[%5ju.%04ju] ", (uintmax_t)(timestamp_s), (uintmax_t)(timestamp_msx));
        vformat(&kprintf_format_func, &st, fmt, ap);

        if(st.last != LF) {
            kprintf_format_func(CR, &st);
            kprintf_format_func(LF, &st);
        }
    }

    if(st.size) {
        console_write(st.data, st.size);
    }

    spinlock_release(&kprintf_lock);
}

define_initcall(kprintf_cmdline, INITCALL_STAGE_BSP_BOOTINFO, INITCALL_ORDER_LAST)
{
    const char* argument = cmdline_argument(&sys_cmdline, "loglevel");

    if(argument) {
        if(0 == strcasecmp(argument, "debug")) {
            loglevel = KP_DEBUG;
        }
        else if(0 == strcasecmp(argument, "inform")) {
            loglevel = KP_INFORM;
        }
        else if(0 == strcasecmp(argument, "notice")) {
            loglevel = KP_NOTICE;
        }
        else if(0 == strcasecmp(argument, "warning")) {
            loglevel = KP_WARNING;
        }
        else if(0 == strcasecmp(argument, "error")) {
            loglevel = KP_ERROR;
        }
        else if(0 == strcasecmp(argument, "critical")) {
            loglevel = KP_CRITICAL;
        }
        else if(0 == strcasecmp(argument, "alert")) {
            loglevel = KP_ALERT;
        }
    }
}
