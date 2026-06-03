#include <core/errno.h>
#include <fs/vfs_mount.h>
#include <fs/vfs_node.h>
#include <fs/vfs_path.h>
#include <fs/vfs_types.h>
#include <lib/string.h>
#include <lib/strings.h>

static int path_walk(struct vfs_node* restrict base, const char* restrict path, struct vfs_node** restrict out)
{
    char component[VFS_NAME_SIZE];
    struct vfs_node* mounted_root;
    struct vfs_node* current = base;
    struct vfs_node* next;
    const char* ptr = path;
    const char* slash;
    size_t length;
    int retval;

    vfs_node_inc_refcount(current);

    while(ptr[0]) {
        while(ptr[0] == PATH_SEPARATOR) {
            ptr += 1;
        }

        if(ptr[0] == 0) {
            break;
        }

        slash = strchr(ptr, PATH_SEPARATOR);

        if(slash == nullptr) {
            length = strlen(ptr);
        }
        else {
            length = (size_t)(slash - ptr);
        }

        if(length > VFS_NAME_MAX) {
            vfs_node_dec_refcount(current);
            return -ENAMETOOLONG;
        }

        memcpy(component, ptr, length);
        component[length] = 0x00;
        ptr += length;

        if(current->type != VFS_NODE_TYPE_DIRECTORY) {
            vfs_node_dec_refcount(current);
            return -ENOTDIR;
        }

        if(current->ops->lookup == nullptr) {
            vfs_node_dec_refcount(current);
            return -ENOSYS;
        }

        next = nullptr;
        retval = current->ops->lookup(current, component, &next);

        vfs_node_dec_refcount(current);

        if(retval < 0) {
            return retval;
        }

        if(next->fs_mounted) {
            mounted_root = next->fs_mounted->root;
            vfs_node_inc_refcount(mounted_root);
            vfs_node_dec_refcount(next);
            next = mounted_root;
        }

        current = next;
    }

    out[0] = current;

    return 0;
}

int vfs_lookup(const char* restrict path, struct vfs_node** restrict out)
{
    struct vfs_node* root;
    struct vfs_node* mounted_root;
    int retval;

    if(path == nullptr || out == nullptr) {
        return -EINVAL;
    }

    root = nullptr;
    retval = vfs_get_root(&root);

    if(retval < 0) {
        return retval;
    }

    if(path[0] == PATH_SEPARATOR) {
        if(root->fs_mounted) {
            mounted_root = root->fs_mounted->root;
            vfs_node_inc_refcount(mounted_root);
            vfs_node_dec_refcount(root);
            root = mounted_root;
        }

        return path_walk(root, path + 1, out);
    }

    return -EINVAL;
}

int vfs_lookup_at(struct vfs_node* restrict base, const char* restrict path, struct vfs_node** restrict out)
{
    if(base == nullptr || path == nullptr || out == nullptr) {
        return -EINVAL;
    }

    if(path[0] == PATH_SEPARATOR) {
        return vfs_lookup(path, out);
    }

    return path_walk(base, path, out);
}
