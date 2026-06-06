#include <core/assert.h> // kassert
#include <fs/vfs_mount.h>
#include <fs/vfs_path.h>
#include <fs/vfs_vnode.h>
#include <lib/string.h>
#include <lib/strings.h>
#include <mm/pslab.h>

#define MAX_SYMLINK_DEPTH 8

static int resolve_internal(struct vnode* restrict stem, const char* restrict path, struct vnode** restrict out, unsigned depth);

static int follow_symlink(struct vnode* restrict node, struct vnode* restrict link, const char* restrict rest, struct vnode** restrict out,
    unsigned depth)
{
    if(depth > MAX_SYMLINK_DEPTH) {
        return -ELOOP;
    }

    char* target = pslab_alloc_virt();

    if(target == nullptr) {
        return -ENOMEM;
    }

    ssize_t length = vnode_readlink(link, target, PATH_MAX);

    if(length < 0) {
        pslab_free_virt(target);
        return length;
    }

    if(length > PATH_MAX) {
        pslab_free_virt(target);
        return -ENAMETOOLONG;
    }

    target[length] = 0;

    struct vnode* base = nullptr;
    int retval;

    if(target[0] == PATH_SEPARATOR) {
        retval = vfs_get_root(&base);

        if(retval < 0) {
            pslab_free_virt(target);
            return retval;
        }
    }
    else {
        base = node;

        vnode_incref(base);
    }

    struct vnode* resolved = nullptr;

    if(target[0] == PATH_SEPARATOR) {
        retval = resolve_internal(base, &target[1], &resolved, depth + 1);
    }
    else {
        retval = resolve_internal(base, target, &resolved, depth + 1);
    }

    vnode_decref(base);

    pslab_free_virt(target);

    if(retval < 0) {
        return retval;
    }

    if(rest && *rest) {
        retval = resolve_internal(resolved, rest, out, depth + 1);
        vnode_decref(resolved);
        return retval;
    }

    *out = resolved;

    return 0;
}

static int resolve_internal(struct vnode* restrict stem, const char* restrict path, struct vnode** restrict out, unsigned depth)
{
    char component[PATH_NAME_SIZE];

    kassert(stem);
    kassert(path);
    kassert(out);

    const char* chrptr = path;
    struct vnode* current = stem;

    vnode_incref(current);

    while(*chrptr) {
        while(*chrptr == PATH_SEPARATOR) {
            chrptr += 1;
        }

        if(*chrptr == 0) {
            break;
        }

        const char* sep = strchr(chrptr, PATH_SEPARATOR);

        ptrdiff_t length;

        if(sep == nullptr) {
            length = strlen(chrptr);
        }
        else {
            length = sep - chrptr;
        }

        if(length > PATH_NAME_MAX) {
            vnode_decref(current);
            return -ENOTDIR;
        }

        memcpy(component, chrptr, length);
        component[length] = 0;
        chrptr += length;

        if(current->nodetype != VNODE_DIRECTORY) {
            vnode_decref(current);
            return -ENOTDIR;
        }

        if(component[0] == '.' && component[1] == 0) {
            continue;
        }

        if(component[0] == '.' && component[1] == '.' && component[2] == 0) {
            struct vnode* upper;

            if(current->fs && current->fs->root == current) {
                if(current->fs->mountpoint) {
                    upper = current->fs->mountpoint;
                }
                else {
                    upper = current;
                }
            }
            else if(current->parent) {
                upper = current->parent;
            }
            else {
                upper = current;
            }

            vnode_incref(upper);
            vnode_decref(current);
            current = upper;
            continue;
        }

        struct vnode* next = nullptr;
        int retval = vnode_lookup(current, component, &next);

        if(retval < 0) {
            vnode_decref(current);
            return retval;
        }

        if(next->nodetype == VNODE_SYM_LINK) {
            retval = follow_symlink(current, next, chrptr, out, depth);
            vnode_decref(current);
            vnode_decref(next);
            return retval;
        }

        vnode_decref(current);
        current = next;
    }

    *out = current;

    return 0;
}

int path_resolve(const char* restrict path, struct vnode** restrict out)
{
    kassert(path);
    kassert(out);

    struct vnode* root = nullptr;
    int retval = vfs_get_root(&root);

    if(retval < 0) {
        return retval;
    }

    if(path[0] == PATH_SEPARATOR) {
        retval = resolve_internal(root, &path[1], out, 0);
        vnode_decref(root);
        return retval;
    }

    vnode_decref(root);
    return -EINVAL;
}

int path_resolve_at(struct vnode* restrict stem, const char* restrict path, struct vnode** restrict out)
{
    kassert(stem);
    kassert(path);
    kassert(out);

    if(path[0] == PATH_SEPARATOR) {
        return path_resolve(path, out);
    }

    return resolve_internal(stem, path, out, 0);
}
