#include <arch/hart.h>
#include <arch/interrupts.h>
#include <core/assert.h>
#include <core/cdefs.h>
#include <core/cmdline.h>
#include <core/kprintf.h>
#include <core/version.h>
#include <dev/blk_device.h>
#include <fs/vfs_dirent.h>
#include <fs/vfs_mount.h>
#include <fs/vfs_path.h>
#include <fs/vfs_vnode.h>
#include <init/initcall.h>
#include <lib/sprintf.h>
#include <lib/string.h>
#include <limine.h>

static volatile __used __section(".limine_requests") uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);
static volatile __used __section(".limine_requests_start") uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;
static volatile __used __section(".limine_requests_end") uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

#include <fs/devfs.h>

static void recurse_dir(struct vnode* restrict node, const char* restrict path)
{
    struct dirent entry;
    struct __k_stat stat;
    size_t index = 0;

    while(0 == vnode_iterate(node, &entry, index++)) {
        if(0 == strcmp(entry.name, ".") || 0 == strcmp(entry.name, "..")) {
            continue;
        }

        const char* codename;

        switch(entry.nodetype) {
            case VNODE_FILE:
                codename = "FIL";
                break;

            case VNODE_DIRECTORY:
                codename = "DIR";
                break;

            case VNODE_BLOCK_DEV:
                codename = "BLK";
                break;

            case VNODE_CHAR_DEV:
                codename = "CHR";
                break;

            case VNODE_SYM_LINK:
                codename = "LNK";
                break;

            default:
                codename = "---";
                break;
        }

        memset(&stat, 0, sizeof(struct __k_stat));
        int chk = vnode_stat(node, &stat);

        if(chk == -ENOSYS) {
            pr_warning("%s/%s: stat: %s", path, entry.name, strerror(chk));
        }

        pr_inform("  %04X [%s] %s/%s", stat.st_mode, codename, path, entry.name);

        if(entry.nodetype == VNODE_DIRECTORY) {
            struct vnode* child;

            if(0 == path_resolve_at(node, entry.name, &child)) {
                char subpath[PATH_NAME_SIZE];
                snprintf(subpath, sizeof(subpath), "%s%c%s", path, PATH_SEPARATOR, entry.name);
                recurse_dir(child, subpath);
                vnode_decref(child);
            }
        }
    }
}

static void recurse_root(void)
{
    struct vnode* root = nullptr;
    if(0 == path_resolve("/", &root)) {
        recurse_dir(root, "");
        vnode_decref(root);
    }
    else {
        pr_warning("no root");
    }
}

void __noreturn kmain(void)
{
    pr_emerg("%s %s %s", k_sysname, k_release, k_version);

    invoke_initcalls(initcalls_bsp_start, initcalls_bsp_end);

    enable_interrupts();

    pr_inform("hanging around...");

    vfs_mount("fat", "/", blk_device_find("ada0n1p1"), nullptr);

    struct vnode* root_node = nullptr;
    struct vnode* dev_node = nullptr;

    if(0 == vfs_get_root(&root_node)) {
        vnode_create(root_node, "dev", VNODE_DIRECTORY, &dev_node);
        vnode_decref(root_node);

        if(dev_node != nullptr) {
            vnode_decref(dev_node);
        }
    }

    vfs_sync("/");
    vfs_mount("devfs", "/dev", nullptr, nullptr);

    recurse_root();

    for(;;) {
        stop_hart();
    }

    unreachable();
}
