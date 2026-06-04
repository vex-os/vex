#include <core/assert.h>
#include <core/cmdline.h>
#include <core/spinlock.h>
#include <fs/vfs_mount.h>
#include <fs/vfs_path.h>
#include <fs/vfs_vcache.h>
#include <fs/vfs_vnode.h>
#include <lib/string.h>
#include <mm/malloc.h>
#include <mm/slab.h>

struct mount_entry {
    struct mount_entry* next;
    struct vfs* fs;
    char* path;
};

static spinlock_type drivers_lock = SPINLOCK_INIT;
static struct vfs_driver* drivers_head = nullptr;

static spinlock_type mounts_lock = SPINLOCK_INIT;
static struct mount_entry* mounts_head = nullptr;
static struct vnode* vfs_root = nullptr;

static struct vfs_driver* find_driver(const char* restrict name)
{
    struct vfs_driver* it;

    for(it = drivers_head; it; it = it->priv.next) {
        if(0 == strcmp(it->identifier, name)) {
            return it;
        }
    }

    return nullptr;
}

int vfs_driver_register(struct vfs_driver* restrict driver)
{
    if(driver == nullptr || driver->ops == nullptr) {
        return -EINVAL;
    }

    if(driver->ops->mount == nullptr || driver->ops->unmount == nullptr) {
        return -EINVAL;
    }

    if(driver->identifier == nullptr || driver->identifier[0] == 0) {
        return -EINVAL;
    }

    spinlock_acquire(&drivers_lock);

    if(find_driver(driver->identifier)) {
        spinlock_release(&drivers_lock);
        return -EEXIST;
    }

    driver->priv.next = drivers_head;
    drivers_head = driver;

    spinlock_release(&drivers_lock);

    return 0;
}

int vfs_driver_unregister(struct vfs_driver* restrict driver)
{
    struct vfs_driver** prev;

    if(driver == nullptr) {
        return -EINVAL;
    }

    spinlock_acquire(&drivers_lock);

    for(prev = &drivers_head; prev[0]; prev = &prev[0]->priv.next) {
        if(prev[0] == driver) {
            prev[0] = driver->priv.next;
            driver->priv.next = nullptr;

            spinlock_release(&drivers_lock);

            return 0;
        }
    }

    spinlock_release(&drivers_lock);

    return -ENOENT;
}

int vfs_mount(const char* restrict type, const char* restrict path, const struct blk_device* restrict device, const char* restrict params)
{
    if(type == nullptr || type[0] == 0) {
        return -EINVAL; // TODO: try deducing filesystem type
    }

    if(path == nullptr) {
        return -EINVAL;
    }

    spinlock_acquire(&drivers_lock);

    struct vfs_driver* driver = find_driver(type);

    spinlock_release(&drivers_lock);

    if(driver == nullptr) {
        return -ENOENT;
    }

    struct cmdline cmdl;
    cmdl.buffer = nullptr;
    cmdl.length = 0;

    if(params) {
        cmdl.buffer = kstrdup(params);

        if(cmdl.buffer) {
            cmdline_parse(&cmdl, cmdl.buffer, 1 + strlen(params));
        }
    }

    struct vfs* fs = slab_calloc(1, sizeof(struct vfs));

    if(fs == nullptr) {
        if(cmdl.buffer)
            kfree(cmdl.buffer);
        return -ENOMEM;
    }

    int retval = driver->ops->mount(fs, device, &cmdl);

    if(cmdl.buffer) {
        kfree(cmdl.buffer);
    }

    if(retval < 0) {
        slab_free(fs);
        return retval;
    }

    fs->ops = driver->ops;

    struct vnode* mountpoint = nullptr;

    if(path[0] != PATH_SEPARATOR || path[1] != 0) {
        retval = path_resolve(path, &mountpoint);

        if(retval < 0) {
            vcache_evict(fs);

            driver->ops->unmount(fs);

            slab_free(fs);

            return retval;
        }
    }

    spinlock_acquire(&mounts_lock);

    if(mountpoint == nullptr) {
        vfs_root = fs->root;
        vnode_incref(vfs_root);
    }
    else {
        mountpoint->fs_mounted = fs;
        fs->mountpoint = mountpoint;
    }

    struct mount_entry* entry = slab_calloc(1, sizeof(struct mount_entry));

    if(entry == nullptr) {
        spinlock_release(&mounts_lock);

        vcache_evict(fs);

        driver->ops->unmount(fs);

        slab_free(fs);

        return -ENOMEM;
    }

    entry->path = kstrdup(path);

    if(entry->path == nullptr) {
        spinlock_release(&mounts_lock);

        vcache_evict(fs);

        driver->ops->unmount(fs);

        slab_free(entry);
        slab_free(fs);

        return -ENOMEM;
    }

    entry->fs = fs;
    entry->next = mounts_head;
    mounts_head = entry;

    spinlock_release(&mounts_lock);

    return 0;
}

int vfs_unmount(const char* restrict path)
{
    weak_kassert_text(path, "invalid path pointer");

    spinlock_acquire(&mounts_lock);

    struct mount_entry** prev = &mounts_head;

    while(prev[0]) {
        struct mount_entry* entry = prev[0];

        if(entry->path && 0 == strcmp(entry->path, path)) {
            struct vfs* fs = entry->fs;
            int retval;

            if(fs->mountpoint) {
                fs->mountpoint->fs_mounted = nullptr;

                vnode_decref(fs->mountpoint);

                fs->mountpoint = nullptr;
            }

            vcache_evict(fs);

            retval = fs->ops->unmount(fs);

            if(retval < 0) {
                spinlock_release(&mounts_lock);
                return retval;
            }

            *prev = entry->next;

            if(vfs_root == fs->root) {
                vnode_decref(vfs_root);
                vfs_root = nullptr;
            }

            slab_free(fs);
            kfree(entry->path);
            slab_free(entry);

            spinlock_release(&mounts_lock);

            return 0;
        }

        prev = &entry->next;
    }

    spinlock_release(&mounts_lock);

    return -ENOENT;
}

int vfs_sync(const char* restrict path)
{
    weak_kassert_text(path, "invalid path pointer");

    spinlock_acquire(&mounts_lock);

    for(struct mount_entry* entry = mounts_head; entry; entry = entry->next) {
        if(entry->path && 0 == strcmp(entry->path, path)) {
            struct vfs* fs = entry->fs;

            spinlock_release(&mounts_lock);

            if(fs->ops->sync == nullptr) {
                return -ENOSYS;
            }

            return fs->ops->sync(fs);
        }
    }

    spinlock_release(&mounts_lock);

    return -ENOENT;
}

struct vfs* vfs_find_mount_for(struct vnode* restrict node)
{
    weak_kassert_text(node, "invalid node pointer");

    spinlock_acquire(&mounts_lock);

    for(struct mount_entry* entry = mounts_head; entry; entry = entry->next) {
        struct vnode* mp = entry->fs->mountpoint;

        if(mp == nullptr) {
            continue;
        }

        if(mp == node) {
            struct vfs* result = entry->fs;
            spinlock_release(&mounts_lock);
            return result;
        }
    }

    spinlock_release(&mounts_lock);

    return nullptr;
}

int vfs_get_root(struct vnode** restrict out)
{
    weak_kassert_text(out, "invalid output pointer");

    spinlock_acquire(&mounts_lock);

    if(vfs_root == nullptr) {
        spinlock_release(&mounts_lock);
        return -ENOENT;
    }

    vnode_incref(vfs_root);

    out[0] = vfs_root;

    spinlock_release(&mounts_lock);

    return 0;
}
