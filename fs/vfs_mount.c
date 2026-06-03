#include <core/assert.h>
#include <core/cmdline.h>
#include <core/errno.h>
#include <core/spinlock.h>
#include <fs/vfs_mount.h>
#include <fs/vfs_node.h>
#include <fs/vfs_path.h>
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
static struct vfs_node* vfs_root = nullptr;

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
    if(driver == nullptr || driver->identifier[0] == 0x00 || driver->ops == nullptr) {
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

int vfs_mount(const char* restrict type, const char* restrict path, struct blk_device* restrict device, const char* restrict params)
{
    struct vfs_node* mountpoint;
    struct vfs_driver* driver;
    struct mount_entry* entry;
    struct cmdline cmdl;
    char* params_copy;
    struct vfs* fs;
    int retval;

    if(type == nullptr || type[0] == 0x00) {
        return -EINVAL;
    }

    spinlock_acquire(&drivers_lock);

    driver = find_driver(type);

    spinlock_release(&drivers_lock);

    if(driver == nullptr) {
        return -ENODEV;
    }

    cmdl.buffer = nullptr;
    cmdl.length = 0;

    if(params) {
        params_copy = kstrdup(params);

        if(params_copy) {
            cmdline_parse(&cmdl, params_copy, 1 + strlen(params));
        }
    }

    fs = slab_alloc(sizeof(struct vfs));

    if(fs == nullptr) {
        if(cmdl.buffer)
            kfree(cmdl.buffer);
        return -ENOMEM;
    }

    memset(fs, 0, sizeof(struct vfs));

    fs->ops = driver->ops;

    retval = driver->ops->mount(fs, device, &cmdl);

    if(cmdl.buffer) {
        kfree(cmdl.buffer);
    }

    if(retval < 0) {
        slab_free(fs);
        return retval;
    }

    spinlock_acquire(&mounts_lock);

    if(vfs_root == nullptr && (path == nullptr || (path[0] == PATH_SEPARATOR && path[1] == 0x00))) {
        vfs_root = fs->root;

        vfs_node_inc_refcount(vfs_root);
    }
    else {
        mountpoint = nullptr;
        retval = vfs_lookup(path, &mountpoint);

        if(retval < 0) {
            spinlock_release(&mounts_lock);

            driver->ops->unmount(fs);

            slab_free(fs);

            return retval;
        }

        mountpoint->fs_mounted = fs;
        fs->mountpoint = mountpoint;
    }

    entry = slab_alloc(sizeof(struct mount_entry));

    if(entry == nullptr) {
        spinlock_release(&mounts_lock);

        driver->ops->unmount(fs);

        slab_free(fs);

        return -ENOMEM;
    }

    memset(entry, 0, sizeof(struct mount_entry));

    if(path) {
        entry->path = kstrdup(path);

        if(entry->path == nullptr) {
            spinlock_release(&mounts_lock);
            driver->ops->unmount(fs);
            slab_free(entry);
            slab_free(fs);
            return -ENOMEM;
        }
    }

    entry->fs = fs;
    entry->next = mounts_head;
    mounts_head = entry;

    spinlock_release(&mounts_lock);

    return 0;
}

int vfs_unmount(const char* restrict path)
{
    struct mount_entry** prev;
    struct mount_entry* entry;
    struct vfs* fs;
    int retval;

    if(path == nullptr) {
        return -EINVAL;
    }

    spinlock_acquire(&mounts_lock);

    prev = &mounts_head;

    while(prev[0]) {
        entry = prev[0];

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

            prev[0] = entry->next;

            if(vfs_root == fs->root) {
                vfs_node_dec_refcount(vfs_root);
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

int vfs_get_root(struct vfs_node** restrict out)
{
    if(out == nullptr) {
        return -EINVAL;
    }

    spinlock_acquire(&mounts_lock);

    if(vfs_root == nullptr) {
        spinlock_release(&mounts_lock);
        return -ENOENT;
    }

    vfs_node_inc_refcount(vfs_root);

    out[0] = vfs_root;

    spinlock_release(&mounts_lock);

    return 0;
}
