#include <arch/hart.h>
#include <arch/interrupts.h>
#include <core/assert.h>
#include <core/cdefs.h>
#include <core/cmdline.h>
#include <core/kprintf.h>
#include <core/version.h>
#include <dev/blk_device.h>
#include <fs/dirent.h>
#include <fs/file.h>
#include <fs/mount.h>
#include <fs/path.h>
#include <fs/vnode.h>
#include <init/initcall.h>
#include <lib/sprintf.h>
#include <lib/string.h>
#include <lib/strings.h>
#include <limine.h>
#include <mm/malloc.h>

static volatile __used __section(".limine_requests") uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);
static volatile __used __section(".limine_requests_start") uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;
static volatile __used __section(".limine_requests_end") uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void recurse_dir(struct vnode* restrict node, const char* restrict path)
{
    char subpath[PATH_NAME_SIZE];
    size_t index = 0;
    struct dirent entry;
    struct vnode* child;

    while(0 == vnode_iterate(node, &entry, index++)) {
        if(0 == strcmp(entry.name, ".") || 0 == strcmp(entry.name, "..")) {
            continue;
        }

        pr_inform("  [%u] %s/%s", entry.nodetype, path, entry.name);

        if(entry.nodetype == VNODE_DIRECTORY) {
            if(0 == path_resolve_at(node, entry.name, &child)) {
                snprintf(subpath, sizeof(subpath), "%s%c%s", path, PATH_SEPARATOR, entry.name);
                recurse_dir(child, subpath);
                vnode_decref(child);
            }
        }
    }
}

static void recurse_root(void)
{
    struct vnode* root;

    if(0 == path_resolve("/", &root)) {
        recurse_dir(root, "/");
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

    vfs_mount("msdos", "/", blk_device_find("ada0n1p1"), "readonly");
    recurse_root();

    struct file* fp = file_open("/limine.conf", O_RDONLY);

    if(fp == nullptr) {
        pr_warning("nofile");
    }
    else {
        char chr[2];
        char buffer[2048];

        memset(buffer, 0, sizeof(buffer));
        chr[1] = 0;

        while(file_read(fp, &chr[0], 1) > 0) {
            kstrncat(buffer, chr, sizeof(buffer));
        }

        pr_inform("file:\n%s", buffer);

        pr_warning("close");

        file_close(fp);
    }

    for(;;) {
        stop_hart();
    }

    unreachable();
}
