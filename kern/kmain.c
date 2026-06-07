#include <limine.h>
#include <sprintf.h>
#include <string.h>
#include <sys.arch/hart.h>
#include <sys.arch/intr.h>
#include <sys/assert.h>
#include <sys/blk_device.h>
#include <sys/cdefs.h>
#include <sys/cmdline.h>
#include <sys/initcall.h>
#include <sys/kprintf.h>
#include <sys/limits.h>
#include <sys/sched.h>
#include <sys/version.h>
#include <sys/vfs_dirent.h>
#include <sys/vfs_mount.h>
#include <sys/vfs_path.h>
#include <sys/vfs_vnode.h>

static volatile __used __section(".limine_requests") uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);
static volatile __used __section(".limine_requests_start") uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;
static volatile __used __section(".limine_requests_end") uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void mode_string(mode_t mode, char* restrict out, size_t sz)
{
    if(sz < 11)
        return;

    switch(mode & S_IFMT) {
        case S_IFREG:
            out[0] = '-';
            break;
        case S_IFDIR:
            out[0] = 'd';
            break;
        case S_IFLNK:
            out[0] = 'l';
            break;
        case S_IFBLK:
            out[0] = 'b';
            break;
        case S_IFCHR:
            out[0] = 'c';
            break;
        case S_IFIFO:
            out[0] = 'p';
            break;
        case S_IFSOCK:
            out[0] = 's';
            break;
        default:
            out[0] = '?';
            break;
    }

    out[1] = (mode & S_IRUSR) ? 'r' : '-';
    out[2] = (mode & S_IWUSR) ? 'w' : '-';
    out[3] = (mode & S_IXUSR) ? 'x' : '-';
    out[4] = (mode & S_IRGRP) ? 'r' : '-';
    out[5] = (mode & S_IWGRP) ? 'w' : '-';
    out[6] = (mode & S_IXGRP) ? 'x' : '-';
    out[7] = (mode & S_IROTH) ? 'r' : '-';
    out[8] = (mode & S_IWOTH) ? 'w' : '-';
    out[9] = (mode & S_IXOTH) ? 'x' : '-';
    out[10] = '\0';
}

static void recurse_dir(struct vnode* restrict node, const char* restrict path)
{
    struct dirent entry;
    struct k_stat stat;
    size_t index = 0;

    while(0 == vnode_iterate(node, &entry, index++)) {
        if(0 == strcmp(entry.name, ".") || 0 == strcmp(entry.name, "..")) {
            continue;
        }

        memset(&stat, 0, sizeof(struct k_stat));
        struct vnode* entry_node;

        if(0 == path_resolve_at(node, entry.name, &entry_node)) {
            vnode_stat(entry_node, &stat);
            vnode_decref(entry_node);
        }

        char mode_str[16];
        mode_string(stat.st_mode, mode_str, sizeof(mode_str));

        pr_inform("  %s %s/%s", mode_str, path, entry.name);

        if(entry.nodetype == VNODE_DIRECTORY) {
            struct vnode* child;

            if(0 == path_resolve_at(node, entry.name, &child)) {
                char subpath[NAME_SIZE];
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

    vfs_mount("echfs", "/", find_blk_device("ada0n1p2"), nullptr);

    struct vnode* root_node = nullptr;
    struct vnode* boot_node = nullptr;
    struct vnode* dev1_node = nullptr;
    struct vnode* dev2_node = nullptr;

    if(0 == vfs_get_root(&root_node)) {
        vnode_create(root_node, "boot", VNODE_DIRECTORY, &boot_node);
        vnode_create(root_node, "dev", VNODE_DIRECTORY, &dev1_node);
        vnode_create(root_node, "dev2", VNODE_DIRECTORY, &dev2_node);
        vnode_decref(root_node);

        if(boot_node != nullptr)
            vnode_decref(boot_node);

        if(dev1_node != nullptr)
            vnode_decref(dev1_node);

        if(dev2_node != nullptr)
            vnode_decref(dev2_node);
    }

    vfs_sync("/");

    vfs_mount("devfs", "/dev", nullptr, nullptr);
    vfs_mount("devfs", "/dev2", nullptr, nullptr);

    vfs_mount("fat", "/boot", find_blk_device("ada0n1p1"), nullptr);

    recurse_root();

    struct vnode* usr_node = nullptr;
    struct vnode* local_node = nullptr;
    struct vnode* bin_node = nullptr;

    if(0 == vfs_get_root(&root_node)) {
        if(0 == vnode_create(root_node, "usr", VNODE_DIRECTORY, &usr_node)) {
            if(0 == vnode_create(usr_node, "local", VNODE_DIRECTORY, &local_node)) {
                vnode_create(local_node, "bin", VNODE_DIRECTORY, &bin_node);
                if(bin_node)
                    vnode_decref(bin_node);
                vnode_decref(local_node);
            }
            vnode_decref(usr_node);
        }
        vnode_decref(root_node);
    }

    vfs_sync("/");

    struct vnode* root = nullptr;
    struct vnode* a = nullptr;
    struct vnode* b = nullptr;

    vfs_get_root(&root);

    if(0 == path_resolve("/dev/..", &a)) {
        pr_inform("dotdot: /dev/.. %s /", a == root ? "==" : "!=");
        vnode_decref(a);
    }
    else {
        pr_warning("dotdot: /dev/.. failed");
    }

    if(0 == path_resolve("/dev/../..", &b)) {
        pr_inform("dotdot: /dev/../.. %s /", b == root ? "==" : "!=");
        vnode_decref(b);
    }
    else {
        pr_warning("dotdot: /dev/../.. failed");
    }

    // /usr/local/bin/../.. == /usr
    struct vnode* usr = nullptr;
    struct vnode* c = nullptr;

    if(0 == path_resolve("/usr", &usr)) {
        if(0 == path_resolve("/usr/local/bin/../..", &c)) {
            pr_inform("dotdot: /usr/local/bin/../.. %s /usr", c == usr ? "==" : "!=");
            vnode_decref(c);
        }
        else {
            pr_warning("dotdot: /usr/local/bin/../.. failed");
        }
        vnode_decref(usr);
    }

    // /usr/local/../local/bin/.. == /usr/local
    struct vnode* local = nullptr;
    struct vnode* d = nullptr;

    if(0 == path_resolve("/usr/local", &local)) {
        if(0 == path_resolve("/usr/local/../local/bin/..", &d)) {
            pr_inform("dotdot: /usr/local/../local/bin/.. %s /usr/local", d == local ? "==" : "!=");
            vnode_decref(d);
        }
        else {
            pr_warning("dotdot: /usr/local/../local/bin/.. failed");
        }
        vnode_decref(local);
    }

    // /dev/../usr/local/bin/../../../dev/.. == /
    struct vnode* e = nullptr;

    if(0 == path_resolve("/dev/../usr/local/bin/../../../dev/..", &e)) {
        pr_inform("dotdot: long path %s /", e == root ? "==" : "!=");
        vnode_decref(e);
    }
    else {
        pr_warning("dotdot: long path failed");
    }

    vnode_decref(root);

    pr_emerg("hanging around...");

    for(;;) {
        sched_yield();
        stop_hart();
    }

    unreachable();
}
