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
#include <lib/sprintf.h>
#include <lib/string.h>
#include <lib/strings.h>
#include <limine.h>
#include <mm/malloc.h>

static volatile __used __section(".limine_requests") uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);
static volatile __used __section(".limine_requests_start") uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;
static volatile __used __section(".limine_requests_end") uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void recurse_list(struct vfs_node* restrict node, const char* restrict path)
{
    struct vfs_dirent de;
    off_t index = 0;

    while(node->ops->read_dir && 0 == node->ops->read_dir(node, &de, index)) {
        if(de.name[0] == '.' && (de.name[1] == 0 || (de.name[1] == '.' && de.name[2] == 0))) {
            index += 1;
            continue;
        }

        pr_inform("  [%u] %s/%s", de.type, path, de.name);

        if(de.type == VFS_NODE_TYPE_DIRECTORY) {
            struct vfs_node* child;

            if(0 == vfs_lookup_at(node, de.name, &child)) {
                char subpath[VFS_NAME_SIZE];
                snprintf(subpath, sizeof(subpath), "%s/%s", path, de.name);
                recurse_list(child, subpath);
                vfs_node_dec_refcount(child);
            }
        }

        index++;
    }
}

static void list_root(void)
{
    struct vfs_node* root;

    if(vfs_lookup("/", &root)) {
        pr_error("failed to get root");
        return;
    }

    recurse_list(root, "");
    vfs_node_dec_refcount(root);
}

void __noreturn kmain(void)
{
    pr_emerg("%s %s %s", k_sysname, k_release, k_version);

    invoke_initcalls(initcalls_bsp_start, initcalls_bsp_end);

    enable_interrupts();

    pr_inform("hanging around...");

    vfs_mount("fat", "/", blk_device_find("ada0n1p1"), "readonly");

    list_root();

    struct vfs_file* file = vfs_open("/limine.conf", VFS_O_RDONLY);

    if(file == nullptr) {
        pr_warning("nofile");
    }
    else {
        char chr[2];
        char buffer[2048];

        memset(buffer, 0, sizeof(buffer));
        chr[1] = 0;

        while(vfs_read(file, &chr[0], 1) > 0) {
            kstrncat(buffer, chr, sizeof(buffer));
        }

        pr_inform("file:\n%s", buffer);

        pr_warning("close");

        vfs_close(file);
    }

    // test_devfs_write("/ada0n2p2");
    // test_devfs_read("/ada0n2p2");

    for(;;) {
        stop_hart();
    }

    unreachable();
}
