// SPDX-License-Identifier: BSD-3-Clause

#include <core/assert.h>
#include <core/cdefs.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <core/panic.h>
#include <core/version.h>
#include <init/initfn.h>
#include <lib/string.h>
#include <limine.h>
#include <mm/malloc.h>

static volatile __used __section(".limine_requests") uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);
static volatile __used __section(".limine_requests_start") uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;
static volatile __used __section(".limine_requests_end") uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void call_all_initfns(void)
{
    size_t i;
    int error;

    for(i = 0; &initfns_start[i] < initfns_end; ++i) {
        kprintf(KP_DEBUG, "kmain: calling initfn: %s [%p]", initfns_start[i].name, initfns_start[i].pfn);

        error = initfns_start[i].pfn();

        if(error == 0 || error == ENODEV) {
            continue;
        }

        panic("kmain: initfn: %s: %s", initfns_start[i].name, strerror(error));
        unreachable();
    }
}

static void test_malloc(void)
{
    size_t i;
    char* a;
    char* b;
    char* c;
    char* d;

    kprintf(KP_DEBUG, "malloc: test: basic alloc/write/free");
    a = kmalloc(64);
    kassert(a != nullptr);
    memset(a, 0xAB, 64);
    for(i = 0; i < 64; ++i)
        kassert((unsigned char)a[i] == 0xAB);
    kfree(a);

    kprintf(KP_DEBUG, "malloc: test: kcalloc is zeroed");
    b = kcalloc(1, 128);
    kassert(b != nullptr);
    for(i = 0; i < 128; ++i)
        kassert(b[i] == 0);
    kfree(b);

    kprintf(KP_DEBUG, "malloc: test: reuse after free (coalesce)");
    a = kmalloc(256);
    b = kmalloc(256);
    kassert(a != nullptr);
    kassert(b != nullptr);
    kassert(a != b);
    kfree(a);
    kfree(b);
    // after coalescing we should get the same base address back
    c = kmalloc(512);
    kassert(c != nullptr);
    kassert(c == a);
    kfree(c);

    kprintf(KP_DEBUG, "malloc: test: krealloc grows");
    d = kmalloc(64);
    kassert(d != nullptr);
    memset(d, 0xCD, 64);
    d = krealloc(d, 256);
    kassert(d != nullptr);
    for(i = 0; i < 64; ++i)
        kassert((unsigned char)d[i] == 0xCD);
    kfree(d);

    kprintf(KP_DEBUG, "malloc: test: double free triggers panic");
    d = kmalloc(64);
    kassert(d != nullptr);
    kfree(d);
    kfree(d); // must panic here

    kprintf(KP_NOTICE, "malloc: test: all passed");
}

void __noreturn kmain(void)
{
    kprintf(KP_INFORM, "%s %s %s", k_sysname, k_release, k_version);

    call_all_initfns();

    test_malloc();

    panic("kmain: nothing else to do");
    unreachable();
}
