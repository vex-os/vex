#include <arch/hart.h>
#include <arch/interrupts.h>
#include <core/assert.h>
#include <core/cdefs.h>
#include <core/cmdline.h>
#include <core/kprintf.h>
#include <core/version.h>
#include <dev/block_device.h>
#include <init/initcall.h>
#include <limine.h>
#include <mm/malloc.h>
#include <stdint.h>

static volatile __used __section(".limine_requests") uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);
static volatile __used __section(".limine_requests_start") uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;
static volatile __used __section(".limine_requests_end") uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void dump_lba(struct block_device* restrict device, uintptr_t lba)
{
    uint8_t* sector;
    unsigned i;

    if(device) {
        sector = kmalloc(device->sector_size);
        strict_kassert(sector);

        if(0 == block_device_read(device, lba, 1, sector)) {
            pr_inform("%s LBA%llu:", device->identifier, (unsigned long long)(lba));
            for(i = 0; i < device->sector_size; i += 16) {
                pr_inform("%03x: %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x", i, sector[i + 0],
                    sector[i + 1], sector[i + 2], sector[i + 3], sector[i + 4], sector[i + 5], sector[i + 6], sector[i + 7], sector[i + 8],
                    sector[i + 9], sector[i + 10], sector[i + 11], sector[i + 12], sector[i + 13], sector[i + 14], sector[i + 15]);
            }
        }
        else {
            pr_error("%s: read failed", device->identifier);
        }

        kfree(sector);
    }
}

void __noreturn kmain(void)
{
    pr_emerg("%s %s %s", k_sysname, k_release, k_version);

    invoke_initcalls(initcalls_bsp_start, initcalls_bsp_end);

    enable_interrupts();

    pr_inform("hanging around...");

    dump_lba(block_device_find("ata0"), 0);
    dump_lba(block_device_find("ata0"), 1);
    dump_lba(block_device_find("ata1"), 0);

    for(;;) {
        stop_hart();
    }

    unreachable();
}
