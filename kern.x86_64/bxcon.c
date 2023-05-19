/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <machine/pmio.h>
#include <sys/init.h>
#include <sys/klog.h>

static void bxcon_write(klog_sink_t *restrict sink, const void *restrict s, size_t n)
{
    size_t i;
    const char *sp = s;
    for(i = 0; i < n; pmio_write8(0xE9, sp[i++]));
}

static klog_sink_t bxcon = {
    .next = NULL,
    .write = &bxcon_write,
    .unblank = NULL,
    .ident = "bxcon",
    .data = NULL,
};

static void init_bxcon(void)
{
    if(pmio_read8(0xE9) != 0xE9)
        return;
    register_klog_sink(&bxcon);
}
early_initcall(bxcon, init_bxcon);
