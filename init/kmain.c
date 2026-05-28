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

#include <amd64/lapic.h>
#include <amd64/pmio.h>
#include <arch/interrupts.h>
#include <arch/ireq.h>
#include <bus/isa.h>
#include <bus/pci.h>
#include <core/intctl.h>
#include <core/ireq.h>
#include <limits.h>

static volatile __used __section(".limine_requests") uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);
static volatile __used __section(".limine_requests_start") uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;
static volatile __used __section(".limine_requests_end") uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void call_all_initfns(void)
{
    size_t i;
    int error;

    for(i = 0; &initfns_start[i] < initfns_end; ++i) {
        kprintf(KP_DEBUG, "kmain: calling initfn: %s <%p>", initfns_start[i].name, initfns_start[i].pfn);

        error = initfns_start[i].pfn();

        if(error == 0 || error == ENODEV) {
            continue;
        }

        panic("kmain: initfn: %s: %s", initfns_start[i].name, strerror(error));
        unreachable();
    }
}

static void test_pci_scan(void)
{
    struct pci_device* dev;
    uint32_t id;
    uint8_t header_type;
    uint8_t class_code, subclass, prog_if;
    uint16_t vendor, device_id;
    uint8_t dev_idx, func;
    unsigned found = 0;

    kprintf(KP_DEBUG, "pci: scanning segment=0 bus=0");

    for(dev_idx = 0; dev_idx < 32; ++dev_idx) {
        dev = pci_open(0, 0, dev_idx, 0);
        if(dev == nullptr)
            continue;

        id = pci_read32(dev, 0x00);
        vendor = (uint16_t)(id & 0xFFFF);

        if(vendor == 0xFFFF) {
            pci_close(dev);
            continue;
        }

        header_type = pci_read8(dev, 0x0E);
        pci_close(dev);

        for(func = 0; func < 8; ++func) {
            if(func != 0 && !(header_type & 0x80))
                break;

            dev = pci_open(0, 0, dev_idx, func);
            if(dev == nullptr)
                continue;

            id = pci_read32(dev, 0x00);
            vendor = (uint16_t)(id & 0xFFFF);
            device_id = (uint16_t)(id >> 16);

            if(vendor == 0xFFFF) {
                pci_close(dev);
                continue;
            }

            class_code = pci_read8(dev, 0x0B);
            subclass = pci_read8(dev, 0x0A);
            prog_if = pci_read8(dev, 0x09);

            kprintf(KP_INFORM, "pci: [0:0:%u:%u] vendor=%04x device=%04x class=%02x:%02x:%02x", (unsigned)dev_idx, (unsigned)func,
                (unsigned)vendor, (unsigned)device_id, (unsigned)class_code, (unsigned)subclass, (unsigned)prog_if);

            pci_close(dev);
            ++found;
        }
    }

    kprintf(KP_INFORM, "pci: scan complete, %u device(s) found", found);
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

    kprintf(KP_NOTICE, "malloc: test: all passed");
}

#define PIT_CH0 0x40
#define PIT_CMD 0x43
#define PIT_HZ  1193182
#define TEST_HZ 100

static volatile unsigned pit_ticks = 0;

static void pit_handler(unsigned number, struct interrupt_frame* restrict frame, void* restrict ctx)
{
    (void)number;
    (void)frame;
    (void)ctx;

    if(pit_ticks % TEST_HZ == 0)
        kprintf(KP_DEBUG, "pit: tick %u (~%u seconds)", pit_ticks, pit_ticks / TEST_HZ);

    pit_ticks += 1U;
}

static void test_pit(void)
{
    struct intctl* ctl;
    unsigned vec;
    uint32_t gsi;
    unsigned trigger, polarity;
    uint16_t divisor;
    uint32_t lapic_id;

    lapic_id = lapic_read(LAPIC_REG_ID) >> 24;

    isa_irq_route(0, &gsi, &trigger, &polarity);

    ctl = find_intctl(gsi);
    if(ctl == nullptr)
        panic("test_pit: no intctl for GSI %u", gsi);

    vec = register_ireq_handler(UINT_MAX, pit_handler, nullptr);
    if(vec == UINT_MAX)
        panic("test_pit: no free IRQ vectors");

    divisor = (uint16_t)(PIT_HZ / TEST_HZ);

    // channel 0, lobyte/hibyte, mode 2 (rate generator)
    pmwrite8(PIT_CMD, 0x34);
    pmwrite8(PIT_CH0, (uint8_t)(divisor));
    pmwrite8(PIT_CH0, (uint8_t)(divisor >> 8));

    kprintf(KP_DEBUG, "test_pit: isa irq 0 -> gsi %u, ireq %u (int 0x%02X), lapic_id=%u, %s %s", gsi, vec, vec + ARCH_IREQ_BASE, lapic_id,
        (trigger == TRIGGER_LEVEL) ? "level" : "edge", (polarity == ACTIVE_LOW) ? "active-low" : "active-high");

    intctl_route(ctl, gsi, (uint8_t)(vec + ARCH_IREQ_BASE), lapic_id, trigger, polarity);
    intctl_unmask(ctl, gsi);

    enable_interrupts();
}

void __noreturn kmain(void)
{
    kprintf(KP_INFORM, "%s %s %s", k_sysname, k_release, k_version);

    call_all_initfns();

    test_malloc();

    test_pci_scan();

    test_pit();

    for(;;)
        asm volatile("hlt");

    unreachable();
}
