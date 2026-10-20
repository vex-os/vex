#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/clocksrc.h>
#include <vex/cmdline.h>
#include <vex/console.h>
#include <vex/format.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/panic.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/strings.h>

#ifndef KP_DEFAULT_LOGLEVEL
#define KP_DEFAULT_LOGLEVEL KP_INFORM
#endif

#define LF 0x0A
#define CR 0x0D

#define STAGEBUF_SIZE 32

typedef struct stagebuf_s stagebuf_t;

struct stagebuf_s {
    char data[STAGEBUF_SIZE];
    size_t size;
    int last;
};

size_t kp_writepos = 0;
char kp_buffer[KP_BUFSIZE];
unsigned loglevel = KP_DEFAULT_LOGLEVEL;

static spinlock_t kprintf_lock = SPINLOCK_INIT;

static void kprintf_format_func(int chr, void* restrict arg)
{
    stagebuf_t* stage = arg;
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
    stagebuf_t st = { 0 };

    bool intr_state = disable_interrupts();

    // A panic has stopped the other harts, so a holder of the lock is either stopped or this hart
    bool locked = !atomic_load_n(&panic_in_progress, ATOMIC_ACQUIRE);

    if(locked) {
        raw_spinlock_acquire(&kprintf_lock);
    }

    if(sv <= loglevel) {
        uint64_t timestamp;

        if(locked) {
            timestamp = clock_source_now_ns();
        }
        else {
            timestamp = clock_source_peek_ns();
        }

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

    if(locked) {
        raw_spinlock_release(&kprintf_lock);
    }

    restore_interrupts(intr_state);
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
