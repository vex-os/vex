#include <sys/atomic.h>
#include <sys/kprintf.h>
#include <sys/process.h>
#include <sys/sched.h>
#include <sys/slab.h>
#include <sys/vmm.h>

static pid_t next_pid = 1;

struct uprocess* uprocess_create(struct uprocess* restrict parent)
{
    struct uprocess* proc = slab_alloc(sizeof(struct uprocess));

    if(proc == nullptr) {
        return nullptr;
    }

    struct pagemap* vm = vmm_create();

    if(vm == nullptr) {
        slab_free(proc);
        return nullptr;
    }

    proc->id = atomic_fetch_add(&next_pid, 1, ATOMIC_RELAXED);
    proc->parent = parent;
    proc->threads = nullptr;
    proc->vm = vm;
    proc->refcount = 0;
    proc->dying = false;

    return proc;
}

static void uprocess_free(struct uprocess* restrict proc)
{
    pr_inform("process %d exited", proc->id);
    vmm_release(proc->vm);
    slab_free(proc);
}

void uprocess_exit(struct uprocess* restrict proc)
{
    proc->dying = true;

    // ask all threads to stop
    // each uthread should call uprocess_unref
    // when it dies - then the process is GC-ed as well
    struct uthread* thread = proc->threads;

    while(thread) {
        // TODO: send signal or force-stop each thread

        thread = thread->next;
    }

    if(proc->refcount == 0) {
        uprocess_free(proc);
    }
}

void uprocess_unref(struct uprocess* restrict proc)
{
    unsigned prev = atomic_fetch_sub(&proc->refcount, 1, ATOMIC_RELEASE);

    if(prev == 1 && proc->dying) {
        uprocess_free(proc);
    }
}
