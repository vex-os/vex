#include <core/assert.h>
#include <fs/mount.h>
#include <fs/path.h>
#include <fs/vnode.h>
#include <lib/string.h>
#include <lib/strings.h>

static int path_walk(struct vnode* restrict stem, const char* restrict path, struct vnode** restrict out)
{
    char component[PATH_NAME_SIZE];

    const char* ptr;
    const char* separator;
    int retval;
    size_t length;
    struct vnode* current;
    struct vnode* mounted;
    struct vnode* next;

    weak_kassert_text(stem, "invalid stem pointer");
    weak_kassert_text(path, "invalid path pointer");
    weak_kassert_text(out, "invalid out pointer");

    ptr = path;
    current = stem;

    vnode_incref(current);

    while(*ptr) {
        while(*ptr == PATH_SEPARATOR) {
            ptr += 1;
        }

        if(*ptr == 0) {
            break;
        }

        separator = strchr(ptr, PATH_SEPARATOR);

        if(separator == nullptr) {
            length = strlen(ptr);
        }
        else {
            length = (size_t)(separator - ptr);
        }

        if(length > PATH_NAME_MAX) {
            vnode_decref(current);
            return -ENAMETOOLONG;
        }

        memcpy(component, ptr, length);
        component[length] = 0;
        ptr += length;

        if(current->nodetype != VNODE_DIRECTORY) {
            vnode_decref(current);
            return -ENOTDIR;
        }

        next = nullptr;
        retval = vnode_lookup(current, component, &next);

        vnode_decref(current);

        if(retval < 0) {
            return retval;
        }

        if(next->fs_mounted) {
            mounted = next->fs_mounted->root;
            vnode_incref(mounted);
            vnode_decref(next);
            next = mounted;
        }

        current = next;
    }

    out[0] = current;

    return 0;
}

int path_resolve(const char* restrict path, struct vnode** restrict out)
{
    int retval;
    struct vnode* mounted;
    struct vnode* root;

    weak_kassert_text(path, "invalid path pointer");
    weak_kassert_text(out, "invalid out pointer");

    root = nullptr;
    retval = vfs_get_root(&root);

    if(retval < 0) {
        return retval;
    }

    if(path[0] == PATH_SEPARATOR) {
        if(root->fs_mounted) {
            mounted = root->fs_mounted->root;
            vnode_incref(mounted);
            vnode_decref(root);
            root = mounted;
        }

        return path_walk(root, &path[1], out);
    }

    return -EINVAL;
}

int path_resolve_at(struct vnode* restrict stem, const char* restrict path, struct vnode** restrict out)
{
    weak_kassert_text(stem, "invalid stem poiner");
    weak_kassert_text(path, "invalid path pointer");
    weak_kassert_text(out, "invalid out pointer");

    if(path[0] == PATH_SEPARATOR) {
        return path_resolve(path, out);
    }

    return path_walk(stem, path, out);
}
