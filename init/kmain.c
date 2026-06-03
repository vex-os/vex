#include <arch/hart.h>
#include <arch/interrupts.h>
#include <core/assert.h>
#include <core/cdefs.h>
#include <core/cmdline.h>
#include <core/kprintf.h>
#include <core/version.h>
#include <dev/blk_device.h>
#include <fs/vfs_file.h>
#include <fs/vfs_mount.h>
#include <fs/vfs_node.h>
#include <fs/vfs_path.h>
#include <fs/vfs_types.h>
#include <init/initcall.h>
#include <limine.h>
#include <mm/malloc.h>

static volatile __used __section(".limine_requests") uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);
static volatile __used __section(".limine_requests_start") uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;
static volatile __used __section(".limine_requests_end") uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void __used test_devfs_listroot(void)
{
    struct vfs_node* root;
    struct vfs_dirent de;
    off_t index = 0;

    if(vfs_lookup("/", &root)) {
        pr_error("failed to get root");
        return;
    }

    while(root->ops->read_dir && 0 == root->ops->read_dir(root, &de, index)) {
        pr_inform("  [%u] %s", de.type, de.name);
        index++;
    }

    if(index == 0) {
        pr_inform("  (empty)");
    }

    vfs_node_dec_refcount(root);
}

void __noreturn kmain(void)
{
    pr_emerg("%s %s %s", k_sysname, k_release, k_version);

    invoke_initcalls(initcalls_bsp_start, initcalls_bsp_end);

    enable_interrupts();

    pr_inform("hanging around...");

    vfs_mount("fat", "/", blk_device_find("ada0n1p1"), nullptr);

    test_devfs_listroot();
    // test_devfs_write("/ada0n2p2");
    // test_devfs_read("/ada0n2p2");

    for(;;) {
        stop_hart();
    }

    unreachable();
}
