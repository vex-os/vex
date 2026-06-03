#include <arch/hart.h>
#include <arch/interrupts.h>
#include <core/assert.h>
#include <core/cdefs.h>
#include <core/cmdline.h>
#include <core/kprintf.h>
#include <core/version.h>
#include <dev/blk_device.h>
#include <fs/vfs_file.h>
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
        pr_error("devfs: failed to get root");
        return;
    }

    pr_inform("devfs root listing:");

    while(root->ops->read_dir && 0 == root->ops->read_dir(root, &de, index)) {
        pr_inform("  [%u] %s", de.type, de.name);
        index++;
    }

    if(index == 0)
        pr_inform("  (empty)");

    vfs_node_dec_refcount(root);
}

static void __used test_devfs_write(const char* path)
{
    static const char sig[] = "***VEX_KERNEL_WRITE***";
    struct vfs_file* file;
    ssize_t result;

    file = vfs_file_open(path, VFS_O_WRONLY);

    if(file == nullptr) {
        pr_error("devfs: failed to open %s for writing", path);
        return;
    }

    if(vfs_file_seek(file, 500, VFS_SEEK_SET)) {
        pr_error("devfs: seek failed on %s", path);
        vfs_file_close(file);
        return;
    }

    /* sizeof includes the null terminator — write only the string itself */
    result = vfs_file_write(file, sig, sizeof(sig) - 1);

    if(result < 0) {
        pr_error("devfs: write failed on %s: %d", path, (int)(-result));
    }
    else {
        pr_inform("devfs: wrote %d bytes to %s @ offset 500", (int)result, path);
    }

    vfs_file_close(file);
}

static void __used test_devfs_read(const char* path)
{
    struct vfs_file* file;
    uint8_t buf[512];
    ssize_t result;
    unsigned i;

    file = vfs_file_open(path, VFS_O_RDONLY);

    if(file == nullptr) {
        pr_error("devfs: failed to open %s", path);
        return;
    }

    if(vfs_file_seek(file, 128, VFS_SEEK_SET)) {
        pr_error("devfs: seek failed on %s", path);
        vfs_file_close(file);
        return;
    }

    result = vfs_file_read(file, buf, sizeof(buf));

    if(result < 0) {
        pr_error("devfs: read failed on %s: %d", path, (int)(-result));
        vfs_file_close(file);
        return;
    }

    pr_inform("%s @ offset 128, %d bytes:", path, (int)result);

    for(i = 0; i < (unsigned)result; i += 16) {
        pr_inform("%03x: %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x", i + 128, buf[i + 0], buf[i + 1],
            buf[i + 2], buf[i + 3], buf[i + 4], buf[i + 5], buf[i + 6], buf[i + 7], buf[i + 8], buf[i + 9], buf[i + 10], buf[i + 11],
            buf[i + 12], buf[i + 13], buf[i + 14], buf[i + 15]);
    }

    vfs_file_close(file);
}

void __noreturn kmain(void)
{
    pr_emerg("%s %s %s", k_sysname, k_release, k_version);

    invoke_initcalls(initcalls_bsp_start, initcalls_bsp_end);

    enable_interrupts();

    pr_inform("hanging around...");

    test_devfs_listroot();
    test_devfs_write("/ada0n2p2");
    test_devfs_read("/ada0n2p2");

    for(;;) {
        stop_hart();
    }

    unreachable();
}
