#include <vex/errno.h>
#include <vex/mman.h>
#include <vex/page.h>
#include <vex/pmap.h>
#include <vex/process.h>
#include <vex/syscall.h>
#include <vex/thread.h>
#include <vex/vms.h>

#define KNOWN_PROT  (PROT_READ | PROT_WRITE | PROT_EXEC)
#define KNOWN_FLAGS (MAP_SHARED | MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS)

static unsigned prot_to_vprot(uintptr_t prot)
{
    unsigned vprot = VPROT_NONE;

    if(prot & PROT_READ) {
        vprot |= VPROT_READ;
    }

    if(prot & PROT_WRITE) {
        vprot |= VPROT_WRITE;
    }

    if(prot & PROT_EXEC) {
        vprot |= VPROT_EXEC;
    }

    return vprot;
}

// Rounds the length up to whole pages, failing
// on zero or on a range past the user half
static bool user_range(struct vmspace* restrict vms, uintptr_t start, uintptr_t length, uintptr_t* restrict end)
{
    uintptr_t user_end = pmap_user_end(vms->pmap);

    if(length == 0 || page_offset(start) || start >= user_end || length > user_end - start) {
        return false;
    }

    *end = start + page_align_up(length);

    return *end <= user_end;
}

static intptr_t sys_mmap(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    uintptr_t addr = args[0];
    uintptr_t length = args[1];
    uintptr_t prot = args[2];
    uintptr_t flags = args[3];

    if(length == 0 || length > pmap_user_end(proc->vms->pmap) || (prot & ~KNOWN_PROT) || (flags & ~KNOWN_FLAGS)) {
        return -EINVAL;
    }

    // Shared anonymous memory only differs from private across fork
    if(0 == (flags & MAP_ANONYMOUS) || (flags & (MAP_SHARED | MAP_PRIVATE)) != MAP_PRIVATE) {
        return -ENOSYS;
    }

    unsigned vprot = prot_to_vprot(prot);
    uintptr_t end;

    if(flags & MAP_FIXED) {
        if(!user_range(proc->vms, addr, length, &end)) {
            return -EINVAL;
        }

        int retval = vms_map_anon_replace(proc->vms, addr, end, vprot);

        if(retval < 0) {
            return retval;
        }

        return addr;
    }

    // The address is a hint taken when it is free
    if(addr && user_range(proc->vms, addr, length, &end) && 0 == vms_map_anon(proc->vms, addr, end, vprot)) {
        return addr;
    }

    uintptr_t placed;
    int retval = vms_map_anon_any(proc->vms, page_align_up(length), vprot, &placed);

    if(retval < 0) {
        return retval;
    }

    return placed;
}

static intptr_t sys_munmap(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    uintptr_t end;

    if(!user_range(proc->vms, args[0], args[1], &end)) {
        return -EINVAL;
    }

    return vms_unmap(proc->vms, args[0], end);
}

static intptr_t sys_mprotect(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    uintptr_t end;

    if(!user_range(proc->vms, args[0], args[1], &end) || (args[2] & ~KNOWN_PROT)) {
        return -EINVAL;
    }

    return vms_protect(proc->vms, args[0], end, prot_to_vprot(args[2]));
}

define_syscall(sys_mmap, SYS_MMAP);
define_syscall(sys_munmap, SYS_MUNMAP);
define_syscall(sys_mprotect, SYS_MPROTECT);
