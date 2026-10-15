#include <vex/assert.h>
#include <vex/blk_device.h>
#include <vex/cmdline.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/mount.h>
#include <vex/mutex.h>
#include <vex/path.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/vcache.h>
#include <vex/vnode.h>

struct mount_entry {
    struct mount_entry* next;
    struct vfs* fs;
    char* path;
};

static_assert(sizeof(struct vfs) <= (PAGE_SIZE / 2));
static_assert(sizeof(struct vfs_driver) <= (PAGE_SIZE / 2));

static spinlock_type drivers_lock = SPINLOCK_INIT;
static struct vfs_driver* drivers_head = nullptr;

// The list only changes with both held, so either one is enough to walk it
static struct mutex mounts_mutex = MUTEX_INIT;
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

static struct vfs_driver* deduce_driver(const struct blk_device* restrict device)
{
    struct vfs_driver* it;

    for(it = drivers_head; it; it = it->priv.next) {
        if(it->ops->probe && it->ops->probe(nullptr, device) == 0) {
            return it;
        }
    }

    return nullptr;
}

static int mount_locked(const char* restrict type, const char* restrict path, const struct blk_device* restrict device,
    const char* restrict params)
{
    if(path == nullptr) {
        return -EINVAL;
    }

    spinlock_acquire(&drivers_lock);

    struct vfs_driver* driver;

    if(type == nullptr) {
        driver = deduce_driver(device);
    }
    else {
        driver = find_driver(type);
    }

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
            parse_cmdline(&cmdl, cmdl.buffer, 1 + strlen(params));
        }
    }

    struct vfs* fs = slab_alloc(sizeof(struct vfs));

    if(fs == nullptr) {
        if(cmdl.buffer)
            kfree(cmdl.buffer);
        return -ENOMEM;
    }

    fs->ops = driver->ops;

    int retval = driver->ops->mount(fs, device, &cmdl);

    const char* device_name;

    if(device == nullptr) {
        device_name = "null";
    }
    else {
        device_name = device->identifier;
    }

    if(retval == 0) {
        pr_verbose("vfs: mount %s: mountpoint=%s device=%s", driver->identifier, path, device_name);
    }
    else {
        pr_verbose("vfs: %s: failed to mount: %s; mountpoint=%s device=%s", driver->identifier, strerror(retval), path, device_name);
    }

    if(cmdl.buffer) {
        kfree(cmdl.buffer);
    }

    if(retval < 0) {
        slab_free(fs);
        return retval;
    }

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

    struct mount_entry* entry = slab_alloc(sizeof(struct mount_entry));

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

int add_vfs_driver(struct vfs_driver* restrict driver)
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

int remove_vfs_driver(struct vfs_driver* restrict driver)
{
    struct vfs_driver** prev;

    if(driver == nullptr) {
        return -EINVAL;
    }

    spinlock_acquire(&drivers_lock);

    for(prev = &drivers_head; *prev; prev = &((**prev).priv.next)) {
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
    mutex_acquire(&mounts_mutex);
    int retval = mount_locked(type, path, device, params);
    mutex_release(&mounts_mutex);
    return retval;
}

int vfs_unmount(const char* restrict path)
{
    kassert(path);

    mutex_acquire(&mounts_mutex);

    struct mount_entry** prev = &mounts_head;

    while(*prev && (*prev)->path && 0 != strcmp((*prev)->path, path)) {
        prev = &(*prev)->next;
    }

    struct mount_entry* entry = *prev;

    if(entry == nullptr) {
        mutex_release(&mounts_mutex);
        return -ENOENT;
    }

    struct vfs* fs = entry->fs;
    bool is_root = vfs_root == fs->root;

    if(vcache_in_use(fs, is_root ? 2 : 1)) {
        mutex_release(&mounts_mutex);
        return -EBUSY;
    }

    int retval = vfs_sync_fs(fs);

    if(retval < 0 && retval != -ENOSYS) {
        mutex_release(&mounts_mutex);
        return retval;
    }

    spinlock_acquire(&mounts_lock);

    *prev = entry->next;

    struct vnode* mountpoint = fs->mountpoint;

    if(mountpoint) {
        mountpoint->fs_mounted = nullptr;
        fs->mountpoint = nullptr;
    }

    if(is_root) {
        vfs_root = nullptr;
    }

    spinlock_release(&mounts_lock);

    pr_verbose("vfs: unmounting %s", path);

    if(mountpoint) {
        vnode_decref(mountpoint);
    }

    if(is_root) {
        vnode_decref(fs->root);
    }

    vcache_evict(fs);
    fs->ops->unmount(fs);

    slab_free(fs);
    kfree(entry->path);
    slab_free(entry);

    mutex_release(&mounts_mutex);

    return 0;
}

int vfs_sync_all(void)
{
    int retval = 0;

    mutex_acquire(&mounts_mutex);

    for(struct mount_entry* entry = mounts_head; entry; entry = entry->next) {
        int fs_retval = vfs_sync_fs(entry->fs);

        if(fs_retval < 0 && fs_retval != -ENOSYS && retval == 0) {
            retval = fs_retval;
        }
    }

    mutex_release(&mounts_mutex);

    return retval;
}

int vfs_statvfs(struct vfs* restrict fs, struct k_statvfs* restrict out)
{
    kassert(fs);
    kassert(out);

    if(fs->ops->statvfs == nullptr) {
        return -ENOSYS;
    }

    return fs->ops->statvfs(fs, out);
}

struct vnode* vfs_get_root(void)
{
    spinlock_acquire(&mounts_lock);

    if(vfs_root == nullptr) {
        spinlock_release(&mounts_lock);
        return nullptr;
    }

    vnode_incref(vfs_root);

    spinlock_release(&mounts_lock);

    return vfs_root;
}

struct vnode* vfs_get_mountpoint(struct vfs* restrict fs)
{
    kassert(fs);

    spinlock_acquire(&mounts_lock);

    struct vnode* mountpoint = fs->mountpoint;

    if(mountpoint) {
        vnode_incref(mountpoint);
    }

    spinlock_release(&mounts_lock);

    return mountpoint;
}

struct vfs* vfs_get_fs(struct vnode* restrict node)
{
    kassert(node);

    spinlock_acquire(&mounts_lock);

    for(struct mount_entry* it = mounts_head; it; it = it->next) {
        struct vnode* mountpoint = it->fs->mountpoint;

        if(mountpoint == nullptr) {
            continue;
        }

        if(mountpoint == node) {
            struct vfs* result = it->fs;
            spinlock_release(&mounts_lock);
            return result;
        }
    }

    spinlock_release(&mounts_lock);

    return nullptr;
}
