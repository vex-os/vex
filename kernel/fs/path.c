#include <vex/assert.h>
#include <vex/mount.h>
#include <vex/path.h>
#include <vex/pslab.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/strings.h>
#include <vex/vnode.h>

#define MAX_SYMLINK_DEPTH 8

static int resolve_internal(struct vnode* restrict stem, const char* restrict path, unsigned flags, struct vnode** restrict out,
    unsigned depth);

static int follow_symlink(struct vnode* restrict node, struct vnode* restrict link, const char* restrict rest, unsigned flags,
    struct vnode** restrict out, unsigned depth)
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
        base = vfs_get_root();

        if(base == nullptr) {
            pslab_free_virt(target);
            return -ENOENT;
        }
    }
    else {
        base = node;

        vnode_incref(base);
    }

    struct vnode* resolved = nullptr;
    bool has_rest = rest && *rest;
    unsigned target_flags = flags;

    // Only the last component may stay unfollowed
    if(has_rest) {
        target_flags = 0;
    }

    if(target[0] == PATH_SEPARATOR) {
        retval = resolve_internal(base, &target[1], target_flags, &resolved, depth + 1);
    }
    else {
        retval = resolve_internal(base, target, target_flags, &resolved, depth + 1);
    }

    vnode_decref(base);

    pslab_free_virt(target);

    if(retval < 0) {
        return retval;
    }

    if(has_rest) {
        retval = resolve_internal(resolved, rest, flags, out, depth + 1);
        vnode_decref(resolved);
        return retval;
    }

    *out = resolved;

    return 0;
}

static int resolve_internal(struct vnode* restrict stem, const char* restrict path, unsigned flags, struct vnode** restrict out,
    unsigned depth)
{
    char component[NAME_SIZE];

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

        if(length > NAME_MAX) {
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

                vnode_incref(upper);
            }
            else {
                spinlock_acquire(&current->lock);

                upper = current->parent ? current->parent : current;

                vnode_incref(upper);

                spinlock_release(&current->lock);
            }

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

        // A trailing separator still counts as more path, as POSIX
        // requires "link/" to resolve to the directory behind it
        bool is_last = *chrptr == 0;

        if(next->nodetype == VNODE_SYM_LINK && (!is_last || 0 == (flags & PATH_NOFOLLOW))) {
            retval = follow_symlink(current, next, chrptr, flags, out, depth);
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

    if(path[0] != PATH_SEPARATOR) {
        return -EINVAL;
    }

    return path_resolve_at(nullptr, path, 0, out);
}

int path_resolve_at(struct vnode* restrict stem, const char* restrict path, unsigned flags, struct vnode** restrict out)
{
    kassert(path);
    kassert(out);

    if(path[0] == 0) {
        return -ENOENT;
    }

    if(stem && path[0] != PATH_SEPARATOR) {
        return resolve_internal(stem, path, flags, out, 0);
    }

    struct vnode* root = vfs_get_root();

    if(root == nullptr) {
        return -ENOENT;
    }

    while(path[0] == PATH_SEPARATOR) {
        path += 1;
    }

    int retval = resolve_internal(root, path, flags, out, 0);

    vnode_decref(root);

    return retval;
}

int path_resolve_parent_at(struct vnode* restrict stem, const char* restrict path, struct vnode** restrict out_parent,
    const char** restrict out_name)
{
    kassert(path);
    kassert(out_parent);
    kassert(out_name);

    if(path[0] == 0) {
        return -ENOENT;
    }

    const char* slash = strrchr(path, PATH_SEPARATOR);

    out_name[0] = slash ? &slash[1] : path;

    int retval;

    if(slash == nullptr) {
        retval = path_resolve_at(stem, ".", 0, out_parent);
    }
    else if(slash == path) {
        retval = path_resolve_at(nullptr, "/", 0, out_parent);
    }
    else {
        size_t dir_length = slash - path;

        if(dir_length > PATH_MAX) {
            return -ENAMETOOLONG;
        }

        char* dir_path = pslab_alloc_virt();

        if(dir_path == nullptr) {
            return -ENOMEM;
        }

        memcpy(dir_path, path, dir_length);

        dir_path[dir_length] = 0x00;

        retval = path_resolve_at(stem, dir_path, 0, out_parent);

        pslab_free_virt(dir_path);
    }

    if(retval < 0) {
        return retval;
    }

    if(out_parent[0]->nodetype != VNODE_DIRECTORY) {
        vnode_decref(out_parent[0]);
        return -ENOTDIR;
    }

    return 0;
}
