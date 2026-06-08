#include <format.h>
#include <string.h>
#include <strings.h>
#include <sys/assert.h>
#include <sys/clocksrc.h>
#include <sys/cmdline.h>
#include <sys/console.h>
#include <sys/initcall.h>
#include <sys/kprintf.h>
#include <sys/spinlock.h>

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

    spinlock_acquire(&kprintf_lock);

    if(sv <= loglevel) {
        uint64_t timestamp = clocksrc_nsec();
        uint64_t timestamp_s = timestamp / UINT64_C(1000000000);
        uint64_t timestamp_msx = (timestamp / UINT64_C(100000)) % UINT64_C(10000);

        format(&kprintf_format_func, &st, "[%5ju.%04ju] ", (uintmax_t)timestamp_s, (uintmax_t)timestamp_msx);
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

static void init_kprintf(void)
{
    const char* argument = cmdline_argument(&sys_cmdline, "loglevel");

    if(argument) {
        if(0 == strcasecmp(argument, "verbose")) {
            loglevel = KP_VERBOSE;
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

initcall_bsp_bootinfo(init_kprintf, INIT_LAST);
