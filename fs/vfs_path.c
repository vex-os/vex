#include <core/assert.h>
#include <fs/vfs_mount.h>
#include <fs/vfs_path.h>
#include <fs/vfs_vnode.h>
#include <lib/string.h>
#include <lib/strings.h>

static int path_walk(struct vnode* restrict stem, const char* restrict path, struct vnode** restrict out)
{
    char component[PATH_NAME_SIZE];

    weak_kassert_text(stem, "invalid stem pointer");
    weak_kassert_text(path, "invalid path pointer");
    weak_kassert_text(out, "invalid out pointer");

    const char* ptr = path;
    struct vnode* current = stem;

    vnode_incref(current);

    while(ptr[0]) {
        while(ptr[0] == PATH_SEPARATOR) {
            ptr += 1;
        }

        if(ptr[0] == 0) {
            break;
        }

        const char* separator = strchr(ptr, PATH_SEPARATOR);

        size_t length;

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

        struct vnode* next = nullptr;
        int retval = vnode_lookup(current, component, &next);

        vnode_decref(current);

        if(retval < 0) {
            return retval;
        }

        current = next;
    }

    out[0] = current;

    return 0;
}

int path_resolve(const char* restrict path, struct vnode** restrict out)
{
    weak_kassert_text(path, "invalid path pointer");
    weak_kassert_text(out, "invalid out pointer");

    struct vnode* root = nullptr;
    int retval = vfs_get_root(&root);

    if(retval < 0) {
        return retval;
    }

    if(path[0] == PATH_SEPARATOR) {
        retval = path_walk(root, &path[1], out);
        vnode_decref(root);
        return retval;
    }

    vnode_decref(root);
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
