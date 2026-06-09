#include <vex/atomic.h>
#include <vex/malloc.h>
#include <vex/process.h>
#include <vex/sched.h>
#include <vex/thread.h>
#include <vex/vmm.h>

static pid_t next_pid = 1;

struct process* process_create(void)
{
    struct process* proc = kmalloc(sizeof(struct process));

    if(proc == nullptr) {
        return nullptr;
    }

    proc->vm = vmm_create();

    if(proc->vm == nullptr) {
        kfree(proc);
        return nullptr;
    }

    proc->pid = atomic_fetch_add(&next_pid, 1, ATOMIC_RELAXED);

    proc->parent = nullptr; // TODO: parenting
    proc->threads = nullptr;
    proc->thread_count = 0;
    proc->exit_code = 0;

    return proc;
}

void process_destroy(struct process* restrict proc)
{
    // TODO: nuke threads - if there's any left we should probably
    // return an error code (something like -EBUSY) instead of just
    // nuking a kernel stack from under a live thread

    vmm_release(proc->vm);
    kfree(proc);
}

struct thread* process_spawn_thread(struct process* restrict proc, uintptr_t user_entry, uintptr_t user_stack_top)
{
    struct thread* thread = thread_create_user(proc, user_entry, user_stack_top);

    if(thread == nullptr) {
        return nullptr;
    }

    sched_enqueue(thread);

    return thread;
}
