#include <vex/assert.h>
#include <vex/errno.h>
#include <vex/file.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/panic.h>
#include <vex/path.h>
#include <vex/process.h>
#include <vex/string.h>
#include <vex/syscall.h>
#include <vex/thread.h>

static struct syscall* syscall_table;
static size_t syscall_table_size;

void dispatch_syscall(struct hart_context* restrict regs)
{
    if(regs->syscall_number >= syscall_table_size) {
        regs->syscall_retval = -ENOSYS;
        return;
    }

    if(syscall_table[regs->syscall_number].function == nullptr) {
        regs->syscall_retval = -ENOSYS;
        return;
    }

    this_thread()->syscall_ctx = regs;

    uintptr_t args[6];
    args[0] = regs->syscall_arg_1;
    args[1] = regs->syscall_arg_2;
    args[2] = regs->syscall_arg_3;
    args[3] = regs->syscall_arg_4;
    args[4] = regs->syscall_arg_5;
    args[5] = regs->syscall_arg_6;

    regs->syscall_retval = syscall_table[regs->syscall_number].function(args);
}

int syscall_dirfd(int dirfd, const char* restrict path, struct vnode** restrict out)
{
    struct process* proc = this_thread()->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    if(path[0] == PATH_SEPARATOR) {
        *out = nullptr;
        return 0;
    }

    if(dirfd == AT_FDCWD) {
        *out = process_get_cwd(proc);

        if(*out == nullptr) {
            return -ENOENT;
        }

        return 0;
    }

    struct file* fp = dereference_fd(proc, dirfd);

    if(fp == nullptr) {
        return -EBADF;
    }

    struct vnode* node = fp->node;

    if(node->nodetype != VNODE_DIRECTORY) {
        file_close(fp);
        return -ENOTDIR;
    }

    vnode_incref(node);
    file_close(fp);

    *out = node;

    return 0;
}

intptr_t syscall_with_fd(const uintptr_t* restrict args, intptr_t (*op)(struct file* restrict fp, const uintptr_t* restrict args))
{
    struct process* proc = this_thread()->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    struct file* fp = dereference_fd(proc, args[0]);

    if(fp == nullptr) {
        return -EBADF;
    }

    intptr_t retval = op(fp, args);

    file_close(fp);

    return retval;
}

static void init_syscall(void)
{
    if(&unsorted_syscalls_start[0] == &unsorted_syscalls_end[0]) {
        pr_alert("syscall: no system calls defined?!");
        return;
    }

    unsigned long max_number = 0;

    for(const struct syscall* it = unsorted_syscalls_start; it < unsorted_syscalls_end; it += 1) {
        if(it->number > max_number) {
            max_number = it->number;
        }
    }

    syscall_table_size = max_number + 1;
    syscall_table = kmalloc(syscall_table_size * sizeof(struct syscall));

    if(syscall_table == nullptr) {
        panic("syscall: out of memory");
    }

    for(const struct syscall* it = unsorted_syscalls_start; it < unsorted_syscalls_end; it += 1) {
        kassert(syscall_table[it->number].function == nullptr);
        kassert(syscall_table[it->number].identifier == nullptr);

        memcpy(&syscall_table[it->number], it, sizeof(struct syscall));
    }
}

initcall_bsp_syscall(init_syscall, INIT_FIRST);
