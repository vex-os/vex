#include <vex/atomic.h>
#include <vex/process.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/vmm.h>

#define PROCTABLE_BUCKETS 1024

// Threads and processes are something that gets
// allocated quite a bit of times across the kernel's
// lifetime so it makes sense to use a fast allocator
// for them; the slab allocator is O(1) for both alloc and free but imposes a size limit
static_assert(sizeof(struct process) <= (PAGE_SIZE / 2));

static struct process* proctable[PROCTABLE_BUCKETS];
static spinlock_type proctable_lock;
static pid_t next_pid = 1;

static __always_inline __nodiscard size_t pid_bucket(pid_t pid)
{
    return (size_t)(pid % PROCTABLE_BUCKETS);
}

struct process* process_create(struct process* restrict parent, struct pagemap* restrict vm, const char* restrict name)
{
    struct process* proc = slab_alloc(sizeof(struct process));

    if(proc == nullptr) {
        return nullptr;
    }

    memset(proc, 0, sizeof(struct process));

    proc->pid = atomic_fetch_add(&next_pid, 1, ATOMIC_RELAXED);
    proc->ppid = parent ? parent->pid : 0;
    proc->lock = SPINLOCK_INIT;
    proc->vm = vm;

    if(name) {
        strncpy(proc->name, name, NAME_SIZE - 1);
    }

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
    proc->exit_code = exit_code;

    vmm_release(proc->vm);
    proc->vm = nullptr;

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

    slab_free(proc);
}
