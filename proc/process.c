#include <vex/atomic.h>
#include <vex/malloc.h>
#include <vex/process.h>
#include <vex/sched.h>
#include <vex/signal.h>
#include <vex/thread.h>
#include <vex/vmm.h>
#include <vex/vprot.h>

extern const uint8_t sigreturn_guard[];

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

    uintptr_t guard_phys = vmm_resolve(&sys_pagemap, (uintptr_t)(sigreturn_guard));

    if(guard_phys == 0) {
        vmm_release(proc->vm);
        kfree(proc);
        return nullptr;
    }

    int retval = vmm_map(proc->vm, SIGRETURN_GUARD_VIRT, guard_phys, VPROT_URX);

    if(retval < 0) {
        vmm_release(proc->vm);
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

size_t memcpy_sys_to_proc(struct process* restrict proc, uintptr_t v_dst, const void* restrict s_src, size_t sz)
{
    return memcpy_sys_to_vm(proc->vm, v_dst, s_src, sz);
}

size_t memcpy_proc_to_sys(void* restrict s_dst, const struct process* restrict proc, uintptr_t v_src, size_t sz)
{
    return memcpy_vm_to_sys(s_dst, proc->vm, v_src, sz);
}

size_t memcpy_sys_to_user(uintptr_t v_dst, const void* restrict s_src, size_t sz)
{
    struct hart_local* hart = this_hart();
    struct thread* thread = hart->curr_thread;

    if(thread == nullptr || thread->owner == nullptr) {
        return sz;
    }

    return memcpy_sys_to_vm(thread->owner->vm, v_dst, s_src, sz);
}

size_t memcpy_user_to_sys(void* restrict s_dst, uintptr_t v_src, size_t sz)
{
    struct hart_local* hart = this_hart();
    struct thread* thread = hart->curr_thread;

    if(thread == nullptr || thread->owner == nullptr) {
        return sz;
    }

    return memcpy_vm_to_sys(s_dst, thread->owner->vm, v_src, sz);
}
