#include <core/assert.h>
#include <core/cmdline.h>
#include <core/spinlock.h>
#include <fs/mount.h>
#include <fs/path.h>
#include <fs/vnode.h>
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
    const char** id;

    for(it = drivers_head; it; it = it->priv.next) {
        for(id = it->identifiers; id && id[0]; id += 1) {
            if(0 == strcmp(*id, name)) {
                return it;
            }
        }
    }

    return nullptr;
}

int vfs_driver_register(struct vfs_driver* restrict driver)
{
    const char** id;

    if(driver == nullptr || driver->ops == nullptr) {
        return -EINVAL;
    }

    if(driver->ops->mount == nullptr || driver->ops->unmount == nullptr) {
        return -EINVAL;
    }

    if(driver->identifiers == nullptr || driver->identifiers[0] == nullptr || driver->identifiers[0][0] == 0) {
        return -EINVAL;
    }

    spinlock_acquire(&drivers_lock);

    for(id = driver->identifiers; id && id[0]; id += 1) {
        if(find_driver(*id)) {
            spinlock_release(&drivers_lock);
            return -EEXIST;
        }
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

    for(prev = &drivers_head; *prev; prev = &(*prev)->priv.next) {
        if(*prev == driver) {
            *prev = driver->priv.next;
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
    int retval;
    struct cmdline cmdl;
    struct mount_entry* entry;
    struct vfs_driver* driver;
    struct vnode* mountpoint;
    struct vfs* fs;

    if(type == nullptr || type[0] == 0) {
        return -EINVAL; // TODO: try deducing filesystem type
    }

    if(path == nullptr || device == nullptr) {
        return -EINVAL;
    }

    spinlock_acquire(&drivers_lock);

    driver = find_driver(type);

    spinlock_release(&drivers_lock);

    if(driver == nullptr) {
        return -ENOENT;
    }

    cmdl.buffer = nullptr;
    cmdl.length = 0;

    if(params) {
        cmdl.buffer = kstrdup(params);

        if(cmdl.buffer) {
            cmdline_parse(&cmdl, cmdl.buffer, 1 + strlen(params));
        }
    }

    fs = slab_calloc(1, sizeof(struct vfs));

    if(fs == nullptr) {
        if(cmdl.buffer)
            kfree(cmdl.buffer);
        return -ENOMEM;
    }

    retval = driver->ops->mount(fs, device, &cmdl);

    if(cmdl.buffer) {
        kfree(cmdl.buffer);
    }

    if(retval < 0) {
        slab_free(fs);
        return retval;
    }

    spinlock_acquire(&mounts_lock);

    if(vfs_root == nullptr && path[0] == PATH_SEPARATOR && path[0] == 0) {
        vfs_root = fs->root;

        vnode_incref(vfs_root);
    }
    else {
        mountpoint = nullptr;
        retval = path_resolve(path, &mountpoint);

        if(retval < 0) {
            spinlock_release(&mounts_lock);

            driver->ops->unmount(fs);

            slab_free(fs);

            return retval;
        }

        mountpoint->fs_mounted = fs;
        fs->mountpoint = mountpoint;
    }

    entry = slab_calloc(1, sizeof(struct mount_entry));

    if(entry == nullptr) {
        spinlock_release(&mounts_lock);

        driver->ops->unmount(fs);

        slab_free(fs);

        return -ENOMEM;
    }

    entry->path = kstrdup(path);

    if(entry->path == nullptr) {
        spinlock_release(&mounts_lock);

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
    int retval;
    struct mount_entry* entry;
    struct mount_entry** prev;
    struct vfs* fs;

    weak_kassert_text(path, "invalid path pointer");

    spinlock_acquire(&mounts_lock);

    prev = &mounts_head;

    while(*prev) {
        entry = *prev;

        if(entry->path && 0 == strcmp(entry->path, path)) {
            fs = entry->fs;

            if(fs->mountpoint) {
                fs->mountpoint->fs_mounted = nullptr;
            }

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
