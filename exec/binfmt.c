#include <vex/binfmt.h>
#include <vex/errno.h>
#include <vex/file.h>
#include <vex/kprintf.h>

static struct binfmt* binfmt_head = nullptr;

int add_binfmt(struct binfmt* restrict fmt)
{
    for(struct binfmt* it = binfmt_head; it; it = it->priv.next) {
        if(it == fmt) {
            return -EEXIST;
        }
    }

    pr_verbose("binfmt: adding %s", fmt->identifier);

    fmt->priv.next = binfmt_head;
    binfmt_head = fmt;

    return 0;
}

int remove_binfmt(struct binfmt* restrict fmt)
{
    if(binfmt_head == fmt) {
        binfmt_head = fmt->priv.next;
        return 0;
    }

    struct binfmt* prev = nullptr;

    for(struct binfmt* it = binfmt_head; it; it = it->priv.next) {
        if(it == fmt) {
            if(prev) {
                prev->priv.next = it->priv.next;
            }

            return 0;
        }

        prev = it;
    }

    return -ENOENT;
}

int load_binary(struct process* restrict proc, struct file* restrict fp, const char** restrict argv, const char** restrict envp,
    struct entry_point* restrict out)
{
    uint8_t header[256];
    ssize_t read = file_read(fp, header, sizeof(header));

    file_seek(fp, 0, SEEK_SET);

    if(read < 0) {
        return read;
    }

    for(struct binfmt* it = binfmt_head; it; it = it->priv.next) {
        if(it->ops->probe && it->ops->setup) {
            int retval = it->ops->probe(fp, header, (size_t)read);

            if(retval == -ENOEXEC) {
                continue;
            }

            if(retval < 0) {
                return retval;
            }

            return it->ops->setup(proc, fp, argv, envp, out);
        }
    }

    return -ENOEXEC;
}
