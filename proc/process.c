#include <vex/atomic.h>
#include <vex/event.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/process.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/vms.h>

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
    proc->lock = SPINLOCK_INIT;
    event_init(&proc->evnt);

    if(parent) {
        spinlock_acquire(&parent->lock);

        proc->cnext = parent->children;

        if(parent->children) {
            parent->children->cprev = proc;
        }

        parent->children = proc;

        spinlock_release(&parent->lock);
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
            spinlock_release(&proctable_lock);
            return it;
        }
    }

    spinlock_release(&proctable_lock);

    return nullptr;
}

void process_exit(struct process* proc, int exit_code)
{
    pr_verbose("process[%lu]: exiting with exit_code=%d", (unsigned long)(proc->pid), exit_code);

    proc->exit_code = exit_code;

    event_trigger(&proc->evnt, true);
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
        spinlock_acquire(&parent->lock);

        if(proc->cprev) {
            proc->cprev->cnext = proc->cnext;
        }
        else {
            parent->children = proc->cnext;
        }

        if(proc->cnext) {
            proc->cnext->cprev = proc->cprev;
        }

        spinlock_release(&parent->lock);
    }

    event_deinit(&proc->evnt);
    kfree(proc);
}
