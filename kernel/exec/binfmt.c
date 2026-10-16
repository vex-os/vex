#include <vex/binfmt.h>
#include <vex/errno.h>
#include <vex/file.h>
#include <vex/kprintf.h>
#include <vex/spinlock.h>

static spinlock_type binfmt_lock = SPINLOCK_INIT;
static struct binfmt* binfmt_head = nullptr;

static struct binfmt* next_binfmt(const struct binfmt* restrict fmt)
{
    spinlock_acquire(&binfmt_lock);
    struct binfmt* next = fmt ? fmt->priv.next : binfmt_head;
    spinlock_release(&binfmt_lock);
    return next;
}

int add_binfmt(struct binfmt* restrict fmt)
{
    spinlock_acquire(&binfmt_lock);

    for(struct binfmt* it = binfmt_head; it; it = it->priv.next) {
        if(it == fmt) {
            spinlock_release(&binfmt_lock);
            return -EEXIST;
        }
    }

    fmt->priv.next = binfmt_head;
    binfmt_head = fmt;

    spinlock_release(&binfmt_lock);

    pr_verbose("binfmt: adding %s", fmt->identifier);

    return 0;
}

int remove_binfmt(struct binfmt* restrict fmt)
{
    spinlock_acquire(&binfmt_lock);

    for(struct binfmt** prev = &binfmt_head; *prev; prev = &(*prev)->priv.next) {
        if(*prev == fmt) {
            *prev = fmt->priv.next;
            spinlock_release(&binfmt_lock);
            return 0;
        }
    }

    spinlock_release(&binfmt_lock);

    return -ENOENT;
}

int load_binary(struct vmspace* restrict vms, struct file* restrict fp, const char** restrict argv, const char** restrict envp,
    struct entry_point* restrict out)
{
    uint8_t header[256];
    ssize_t read = file_read(fp, header, sizeof(header));

    file_seek(fp, 0, SEEK_SET);

    if(read < 0) {
        return read;
    }

    for(struct binfmt* it = next_binfmt(nullptr); it; it = next_binfmt(it)) {
        if(it->ops->probe && it->ops->setup) {
            int retval = it->ops->probe(fp, header, (size_t)read);

            if(retval == -ENOEXEC) {
                continue;
            }

            if(retval < 0) {
                return retval;
            }

            return it->ops->setup(vms, fp, argv, envp, out);
        }
    }

    return -ENOEXEC;
}
