#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/errno.h>
#include <vex/event.h>
#include <vex/file.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/mount.h>
#include <vex/process.h>
#include <vex/sched.h>
#include <vex/signal.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/thread.h>
#include <vex/vms.h>
#include <vex/vnode.h>

#define INIT_PID            1
#define PROCTABLE_BUCKETS   1024
#define PROCESS_SIZE_BUDGET 256

static_assert(sizeof(struct process) <= PROCESS_SIZE_BUDGET);

static struct process* proctable[PROCTABLE_BUCKETS];
static spinlock_type proctable_lock = SPINLOCK_INIT; // Taken with interrupts disabled: the scheduler looks up parents
static pid_t next_pid = INIT_PID;

static __always_inline __nodiscard size_t pid_bucket(pid_t pid)
{
    return pid % PROCTABLE_BUCKETS;
}

static int grow_fds_locked(struct process* proc, size_t fd)
{
    if(fd < proc->max_fds) {
        return 0;
    }

    if(fd >= CONFIG_MAX_FDS) {
        return -EMFILE;
    }

    size_t new_max = proc->max_fds ? proc->max_fds * 2 : 4;

    while(new_max <= fd) {
        new_max *= 2;
    }

    if(new_max > CONFIG_MAX_FDS) {
        new_max = CONFIG_MAX_FDS;
    }

    struct file** grown = krealloc(proc->fds, new_max * sizeof(struct file*));

    if(grown == nullptr) {
        return -ENOMEM;
    }

    for(size_t i = proc->max_fds; i < new_max; ++i) {
        grown[i] = nullptr;
    }

    proc->fds = grown;
    proc->max_fds = new_max;

    return 0;
}

static void notify_parent(struct process* restrict proc)
{
    struct process* parent = process_locate(atomic_load_n(&proc->ppid, ATOMIC_SEQ_CST));

    if(parent) {
        event_trigger(&parent->child_exited, true);
        process_decref(parent);
    }
}

// Children are moved before their zombie flags are
// checked while a dying child sets its flag before
// reading its parent: one side or the other always gets to init
static void reparent_children(struct process* restrict proc)
{
    struct process* init = process_locate(INIT_PID);

    if(init == proc) {
        process_decref(init);
        init = nullptr;
    }

    bool intr_state = spinlock_acquire_intr(&proc->lock);

    if(init) {
        spinlock_acquire(&init->lock);
    }

    bool zombie_adopted = false;
    struct process* last = nullptr;

    for(struct process* it = proc->children; it; it = it->cnext) {
        atomic_store_n(&it->ppid, init ? INIT_PID : 0, ATOMIC_SEQ_CST);

        if(atomic_load_n(&it->zombie, ATOMIC_SEQ_CST)) {
            zombie_adopted = true;
        }

        last = it;
    }

    if(init && last) {
        last->cnext = init->children;

        if(init->children) {
            init->children->cprev = last;
        }

        init->children = proc->children;
    }

    proc->children = nullptr;

    if(init) {
        spinlock_release(&init->lock);
    }

    spinlock_release_intr(&proc->lock, intr_state);

    if(init) {
        if(zombie_adopted) {
            event_trigger(&init->child_exited, true);
        }

        process_decref(init);
    }
}

// Closing may sleep in a filesystem, so this runs in the exiting
// thread rather than when the scheduler reaps the last thread
static void release_files(struct process* restrict proc)
{
    process_set_cwd(proc, nullptr);

    spinlock_acquire(&proc->fd_lock);

    struct file** fds = proc->fds;
    size_t max_fds = proc->max_fds;

    proc->fds = nullptr;
    proc->max_fds = 0;
    proc->num_fds = 0;

    spinlock_release(&proc->fd_lock);

    for(size_t i = 0; i < max_fds; ++i) {
        file_close(fds[i]);
    }

    kfree(fds);
}

struct process* process_create(struct process* restrict parent)
{
    struct process* proc = slab_alloc(sizeof(struct process));

    if(proc == nullptr) {
        return nullptr;
    }

    proc->vms = vms_create();

    if(proc->vms == nullptr) {
        slab_free(proc);
        return nullptr;
    }

    struct sighand* parent_sighand = nullptr;

    if(parent) {
        parent_sighand = parent->sighand;
    }

    proc->sighand = sighand_create(parent_sighand);

    if(proc->sighand == nullptr) {
        vms_release(proc->vms);
        slab_free(proc);
        return nullptr;
    }

    if(parent) {
        proc->ppid = parent->pid;
    }

    proc->pid = atomic_fetch_add(&next_pid, 1, ATOMIC_RELAXED);
    proc->refcount = 1;
    proc->lock = SPINLOCK_INIT;
    proc->fd_lock = SPINLOCK_INIT;
    event_init(&proc->evnt);
    event_init(&proc->child_exited);
    mutex_init(&proc->cred_lock);

    if(parent) {
        proc->cred = process_get_cred(parent);
        proc->umask = atomic_load_n(&parent->umask, ATOMIC_RELAXED);
        proc->cwd = process_get_cwd(parent);
    }
    else {
        proc->cred = &sys_cred;
        cred_incref(&sys_cred);
        proc->umask = S_IWGRP | S_IWOTH;
        proc->cwd = nullptr;
    }

    if(proc->cwd == nullptr) {
        proc->cwd = vfs_get_root();
    }

    if(proc->cwd == nullptr) {
        cred_decref(proc->cred);
        sighand_destroy(proc->sighand);
        vms_release(proc->vms);
        slab_free(proc);
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

    bool intr_state = spinlock_acquire_intr(&proctable_lock);

    proc->pnext = proctable[bucket];
    proctable[bucket] = proc;

    spinlock_release_intr(&proctable_lock, intr_state);

    return proc;
}

struct process* process_locate(pid_t pid)
{
    size_t bucket = pid_bucket(pid);

    bool intr_state = spinlock_acquire_intr(&proctable_lock);

    for(struct process* it = proctable[bucket]; it; it = it->pnext) {
        if(it->pid == pid) {
            process_incref(it);
            spinlock_release_intr(&proctable_lock, intr_state);
            return it;
        }
    }

    spinlock_release_intr(&proctable_lock, intr_state);

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

        cred_decref(proc->cred);
        sighand_destroy(proc->sighand);
        mutex_deinit(&proc->cred_lock);
        event_deinit(&proc->evnt);
        event_deinit(&proc->child_exited);
        slab_free(proc);
    }
}

void process_exit(struct process* proc, int exit_code)
{
    if(atomic_exchange(&proc->exiting, true, ATOMIC_ACQ_REL)) {
        return;
    }

    pr_verbose("process[%lu]: exiting with exit_code=%d", (unsigned long)(proc->pid), exit_code);

    proc->exit_code = exit_code;

    bool intr_state = spinlock_acquire_intr(&proc->lock);

    for(struct thread* thrd = proc->threads; thrd; thrd = thrd->tnext) {
        atomic_fetch_or(&thrd->pending_work, THREAD_WORK_EXIT, ATOMIC_RELEASE);

        sched_kick(thrd);
    }

    spinlock_release_intr(&proc->lock, intr_state);

    // A zombie must not keep filesystems
    // busy until it is waited for
    release_files(proc);
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

        atomic_store_n(&proc->zombie, true, ATOMIC_SEQ_CST);
        event_trigger(&proc->evnt, true);

        notify_parent(proc);
        reparent_children(proc);
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

    bool intr_state = spinlock_acquire_intr(&proctable_lock);

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

    spinlock_release_intr(&proctable_lock, intr_state);

    struct process* parent = process_locate(atomic_load_n(&proc->ppid, ATOMIC_SEQ_CST));

    if(parent) {
        intr_state = spinlock_acquire_intr(&parent->lock);

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

    release_files(proc);

    process_decref(proc);
}

struct credentials* process_get_cred(struct process* proc)
{
    bool intr_state = spinlock_acquire_intr(&proc->lock);

    struct credentials* cred = proc->cred;
    cred_incref(cred);

    spinlock_release_intr(&proc->lock, intr_state);

    return cred;
}

void process_set_cred(struct process* proc, struct credentials* cred)
{
    kassert(mutex_held(&proc->cred_lock));

    bool intr_state = spinlock_acquire_intr(&proc->lock);

    struct credentials* old_cred = proc->cred;
    atomic_store(&proc->cred, cred, ATOMIC_RELEASE);

    spinlock_release_intr(&proc->lock, intr_state);

    cred_decref(old_cred);
}

struct vnode* process_get_cwd(struct process* proc)
{
    bool intr_state = spinlock_acquire_intr(&proc->lock);

    struct vnode* cwd = proc->cwd;

    if(cwd) {
        vnode_incref(cwd);
    }

    spinlock_release_intr(&proc->lock, intr_state);

    return cwd;
}

void process_set_cwd(struct process* proc, struct vnode* node)
{
    bool intr_state = spinlock_acquire_intr(&proc->lock);

    struct vnode* old_cwd = proc->cwd;
    proc->cwd = node;

    spinlock_release_intr(&proc->lock, intr_state);

    if(old_cwd) {
        vnode_decref(old_cwd);
    }
}

struct file* dereference_fd(struct process* proc, int fd)
{
    spinlock_acquire(&proc->fd_lock);

    struct file* fp = nullptr;

    if(fd >= 0 && (size_t)(fd) < proc->max_fds) {
        fp = file_dupe(proc->fds[fd]);
    }

    spinlock_release(&proc->fd_lock);

    return fp;
}

int allocate_fd(struct process* proc, struct file* file)
{
    spinlock_acquire(&proc->fd_lock);

    for(size_t fd = 0; fd < proc->max_fds; ++fd) {
        if(proc->fds[fd] == nullptr) {
            proc->fds[fd] = file;
            proc->num_fds += 1;
            spinlock_release(&proc->fd_lock);
            return fd;
        }
    }

    int fd = proc->max_fds;
    int retval = grow_fds_locked(proc, fd);

    if(retval < 0) {
        spinlock_release(&proc->fd_lock);
        return retval;
    }

    proc->fds[fd] = file;
    proc->num_fds += 1;

    spinlock_release(&proc->fd_lock);

    return fd;
}

int duplicate_fd(struct process* proc, int old_fd, int new_fd)
{
    struct file* copy = dereference_fd(proc, old_fd);

    if(copy == nullptr) {
        return -EBADF;
    }

    if(old_fd == new_fd) {
        file_close(copy);
        return new_fd;
    }

    if(new_fd < 0) {
        int fd = allocate_fd(proc, copy);

        if(fd < 0) {
            file_close(copy);
        }

        return fd;
    }

    // As POSIX has it, a descriptor past
    // the limit is a bad one rather than one too many
    if(new_fd >= CONFIG_MAX_FDS) {
        file_close(copy);
        return -EBADF;
    }

    spinlock_acquire(&proc->fd_lock);

    int retval = grow_fds_locked(proc, new_fd);

    if(retval < 0) {
        spinlock_release(&proc->fd_lock);
        file_close(copy);
        return retval;
    }

    struct file* replaced = proc->fds[new_fd];
    proc->fds[new_fd] = copy;

    if(replaced == nullptr) {
        proc->num_fds += 1;
    }

    spinlock_release(&proc->fd_lock);

    // Closing may sleep in the filesystem
    // so it happens outside the lock
    file_close(replaced);

    return new_fd;
}

int release_fd(struct process* proc, int fd)
{
    spinlock_acquire(&proc->fd_lock);

    if(fd < 0 || (size_t)(fd) >= proc->max_fds || proc->fds[fd] == nullptr) {
        spinlock_release(&proc->fd_lock);
        return -EBADF;
    }

    struct file* fp = proc->fds[fd];
    proc->fds[fd] = nullptr;
    proc->num_fds -= 1;

    spinlock_release(&proc->fd_lock);

    file_close(fp);

    return 0;
}

int copy_fds(struct process* restrict proc, struct process* restrict stem)
{
    spinlock_acquire(&stem->fd_lock);

    struct file** fds = nullptr;

    if(stem->max_fds) {
        fds = kmalloc(stem->max_fds * sizeof(struct file*));

        if(fds == nullptr) {
            spinlock_release(&stem->fd_lock);
            return -ENOMEM;
        }

        for(size_t i = 0; i < stem->max_fds; ++i) {
            fds[i] = file_dupe(stem->fds[i]);
        }
    }

    proc->fds = fds;
    proc->max_fds = stem->max_fds;
    proc->num_fds = stem->num_fds;

    spinlock_release(&stem->fd_lock);

    return 0;
}
