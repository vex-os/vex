// SPDX-License-Identifier: BSD-3-Clause
#include <core/cdefs.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <ext/limine.h>
#include <init/initfn.h>
#include <time/clocksource.h>

static volatile __used __section(".limine_requests") struct limine_tsc_frequency_request tsc_freq_request = {
    .id = LIMINE_TSC_FREQUENCY_REQUEST_ID,
    .revision = 0,
};

static uint64_t invtsc_op_read(const struct clocksource* restrict clock)
{
    uint64_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return (hi << 32) | lo;
}

static const struct clocksource_ops invtsc_ops = {
    .read = invtsc_op_read,
};

static struct clocksource invtsc_clocksource = {
    .ops = &invtsc_ops,
    .identity = "tsc-invariant",
    .rating = 200,
    .read_mask = UINT64_MAX,
    .read_mult = UINT64_C(0),
    .read_shift = UINT64_C(0),
};

static int init_invtsc(void)
{
    uint64_t freq_hz;

    if(!tsc_freq_request.response) {
        kprintf(KP_WARNING, "invtsc: Limine did not provide TSC frequency");

        return ENODEV;
    }

    freq_hz = tsc_freq_request.response->frequency;

    if(freq_hz == 0) {
        kprintf(KP_WARNING, "invtsc: Limine reported zero TSC frequency");

        return ENODEV;
    }

    invtsc_clocksource.read_shift = 32;
    invtsc_clocksource.read_mult = UINT64_C(1000000000) << 32;
    invtsc_clocksource.read_mult /= freq_hz;

    return register_clocksource(&invtsc_clocksource);
}

DEFINE_INITFN(init_invtsc, INITFN_STAGE_TIMER, INITFN_ORDER_MIDDLE);
