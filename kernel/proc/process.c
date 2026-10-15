#include <vex/atomic.h>
#include <vex/errno.h>
#include <vex/event.h>
#include <vex/file.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/mount.h>
#include <vex/process.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/thread.h>
#include <vex/vms.h>
#include <vex/vnode.h>

#define PROCTABLE_BUCKETS 1024

static struct process* proctable[PROCTABLE_BUCKETS];
static spinlock_type proctable_lock;
static pid_t next_pid = 1;

static __always_inline __nodiscard size_t pid_bucket(pid_t pid)
{
    return pid % PROCTABLE_BUCKETS;
}

struct process* process_create(struct process* restrict parent)
{
    struct process* proc = kmalloc(sizeof(struct process));

    if(proc == nullptr) {
        return nullptr;
    }

    proc->vms = vms_create();

    if(proc->vms == nullptr) {
        kfree(proc);
        return nullptr;
    }

    if(parent) {
        proc->ppid = parent->pid;
        proc->thread_stack_top = parent->thread_stack_top;
    }
    else {
        proc->thread_stack_top = UINT64_C(0x0000700000000000);
    }

    proc->pid = atomic_fetch_add(&next_pid, 1, ATOMIC_RELAXED);
    proc->refcount = 1;
    proc->lock = SPINLOCK_INIT;
    event_init(&proc->evnt);

    if(parent && parent->cwd) {
        vnode_incref(parent->cwd);
        proc->cwd = parent->cwd;
    }
    else {
        proc->cwd = vfs_get_root();
    }

    if(proc->cwd == nullptr) {
        vms_release(proc->vms);
        kfree(proc);
        return nullptr;
    }

    if(parent) {
        bool intr_state = spinlock_acquire_intr(&parent->lock);

        proc->cnext = parent->children;

        if(parent->children) {
            parent->children->cprev = proc;
        }

        parent->children = proc;

        spinlock_release_intr(&parent->lock, intr_state);
    }

    size_t bucket = pid_bucket(proc->pid);

    spinlock_acquire(&proctable_lock);

    proc->pnext = proctable[bucket];
    proctable[bucket] = proc;

    spinlock_release(&proctable_lock);

    return proc;
}

struct process* process_locate(pid_t pid)
{
    size_t bucket = pid_bucket(pid);

    spinlock_acquire(&proctable_lock);

    for(struct process* it = proctable[bucket]; it; it = it->pnext) {
        if(it->pid == pid) {
            process_incref(it);
            spinlock_release(&proctable_lock);
            return it;
        }
    }

    spinlock_release(&proctable_lock);

    return nullptr;
}

void process_incref(struct process* proc)
{
    atomic_fetch_add(&proc->refcount, 1, ATOMIC_RELAXED);
}

void process_decref(struct process* proc)
{
    if(1 == atomic_fetch_sub(&proc->refcount, 1, ATOMIC_ACQ_REL)) {
        // May run from the scheduler: only interrupt-safe teardown here,
        // everything else was released by process_destroy already

        if(proc->vms) {
            vms_release(proc->vms); // Never got as far as running a thread
        }

        event_deinit(&proc->evnt);
        kfree(proc);
    }
}

void process_exit(struct process* proc, int exit_code)
{
    pr_verbose("process[%lu]: exiting with exit_code=%d", (unsigned long)(proc->pid), exit_code);

    proc->exit_code = exit_code;

    vfs_sync_all(); // a nasty crutch
}

void process_thread_reaped(struct process* proc, struct thread* thrd)
{
    bool intr_state = spinlock_acquire_intr(&proc->lock);

    if(thrd->tprev) {
        thrd->tprev->tnext = thrd->tnext;
    }
    else {
        proc->threads = thrd->tnext;
    }

    if(thrd->tnext) {
        thrd->tnext->tprev = thrd->tprev;
    }

    bool last = (0 == --proc->num_threads);

    spinlock_release_intr(&proc->lock, intr_state);

    if(last) {
        // No thread of the process is on any hart anymore, so
        // nothing has its page tables loaded and they can go
        vms_release(proc->vms);
        proc->vms = nullptr;

        atomic_store_n(&proc->zombie, true, ATOMIC_RELEASE);
        event_trigger(&proc->evnt, true);
    }

    process_decref(proc);
}

bool process_claim(struct process* proc)
{
    bool expected = false;

    if(!atomic_load_n(&proc->zombie, ATOMIC_ACQUIRE)) {
        return false;
    }

    return atomic_cmpxchg(&proc->collected, &expected, true, ATOMIC_ACQ_REL, ATOMIC_RELAXED);
}

void process_destroy(struct process* proc)
{
    size_t bucket = pid_bucket(proc->pid);

    spinlock_acquire(&proctable_lock);

    struct process* prev = nullptr;

    for(struct process* it = proctable[bucket]; it; it = it->pnext) {
        if(it == proc) {
            if(prev) {
                prev->pnext = it->pnext;
            }
            else {
                proctable[bucket] = it->pnext;
            }

            break;
        }

        prev = it;
    }

    spinlock_release(&proctable_lock);

    struct process* parent = process_locate(proc->ppid);

    if(parent) {
        bool intr_state = spinlock_acquire_intr(&parent->lock);

        if(proc->cprev) {
            proc->cprev->cnext = proc->cnext;
        }
        else {
            parent->children = proc->cnext;
        }

        if(proc->cnext) {
            proc->cnext->cprev = proc->cprev;
        }

        spinlock_release_intr(&parent->lock, intr_state);

        process_decref(parent);
    }

    if(proc->cwd) {
        vnode_decref(proc->cwd);
        proc->cwd = nullptr;
    }

    for(size_t i = 0; i < proc->max_fds; i += 1) {
        if(proc->fds[i] != nullptr) {
            file_close(proc->fds[i]);
        }
    }

    kfree(proc->fds);
    proc->fds = nullptr;
    proc->max_fds = 0;
    proc->num_fds = 0;

    process_decref(proc);
}

struct file* dereference_fd(struct process* proc, int fd)
{
    if(fd < 0 || fd >= proc->max_fds) {
        return nullptr;
    }

    return proc->fds[fd];
}

int allocate_fd(struct process* proc, struct file* file)
{
    for(int fd = 0; fd < proc->max_fds; fd += 1) {
        if(proc->fds[fd] == nullptr) {
            proc->fds[fd] = file;
            proc->num_fds += 1;
            return fd;
        }
    }

    size_t new_max_fds = 4;

    if(proc->max_fds > 0) {
        new_max_fds = proc->max_fds * 2;
    }

    struct file** new_fds = krealloc(proc->fds, new_max_fds * sizeof(struct file*));

    if(new_fds == nullptr) {
        return -ENOMEM;
    }

    for(size_t i = proc->max_fds; i < new_max_fds; i += 1) {
        new_fds[i] = nullptr;
    }

    proc->fds = new_fds;
    proc->fds[proc->max_fds] = file;

    int new_fd = proc->max_fds;
    proc->max_fds = new_max_fds;
    proc->num_fds += 1;

    return new_fd;
}

int duplicate_fd(struct process* proc, int old_fd, int new_fd)
{
    struct file* fp = dereference_fd(proc, old_fd);

    if(fp == nullptr) {
        return -EBADF;
    }

    if(old_fd == new_fd) {
        return new_fd;
    }

    struct file* copy = file_dupe(fp);

    if(copy == nullptr) {
        return -ENOMEM;
    }

    if(new_fd < 0) {
        int fd = allocate_fd(proc, copy);

        if(fd < 0) {
            file_close(copy);
        }

        return fd;
    }

    release_fd(proc, new_fd);

    if((size_t)new_fd >= proc->max_fds) {
        size_t new_max = proc->max_fds ? proc->max_fds * 2 : 4;

        while(new_max <= (size_t)new_fd) {
            new_max *= 2;
        }

        struct file** grown = krealloc(proc->fds, new_max * sizeof(struct file*));

        if(grown == nullptr) {
            file_close(copy);
            return -ENOMEM;
        }

        for(size_t i = proc->max_fds; i < new_max; i += 1) {
            grown[i] = nullptr;
        }

        proc->fds = grown;
        proc->max_fds = new_max;
    }

    proc->fds[new_fd] = copy;
    proc->num_fds += 1;

    return new_fd;
}

void release_fd(struct process* proc, int fd)
{
    if(fd < 0 || fd >= proc->max_fds || proc->fds[fd] == nullptr) {
        return;
    }

    file_close(proc->fds[fd]);

    proc->fds[fd] = nullptr;
    proc->num_fds -= 1;
}
