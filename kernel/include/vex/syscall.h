#pragma once

#include <usr/vex/syscall.h>
#include <vex/compiler.h>
#include <vex/types.h>

#define SYS_EXIT      __k_SYS_EXIT
#define SYS_FORK      __k_SYS_FORK
#define SYS_EXEC      __k_SYS_EXEC
#define SYS_WAITPID   __k_SYS_WAITPID
#define SYS_GETPID    __k_SYS_GETPID
#define SYS_GETPPID   __k_SYS_GETPPID
#define SYS_YIELD     __k_SYS_YIELD
#define SYS_OPENAT    __k_SYS_OPENAT
#define SYS_CLOSE     __k_SYS_CLOSE
#define SYS_READ      __k_SYS_READ
#define SYS_WRITE     __k_SYS_WRITE
#define SYS_SEEK      __k_SYS_SEEK
#define SYS_FSTAT     __k_SYS_FSTAT
#define SYS_UNLINKAT  __k_SYS_UNLINKAT
#define SYS_DUP       __k_SYS_DUP
#define SYS_DUP2      __k_SYS_DUP2
#define SYS_IOCTL     __k_SYS_IOCTL
#define SYS_MKDIRAT   __k_SYS_MKDIRAT
#define SYS_READDIR   __k_SYS_READDIR
#define SYS_GETCWD    __k_SYS_GETCWD
#define SYS_CHDIR     __k_SYS_CHDIR
#define SYS_MOUNT     __k_SYS_MOUNT
#define SYS_UMOUNT    __k_SYS_UMOUNT
#define SYS_SYNC      __k_SYS_SYNC
#define SYS_SYNCFS    __k_SYS_SYNCFS
#define SYS_STATVFS   __k_SYS_STATVFS
#define SYS_MMAP      __k_SYS_MMAP
#define SYS_MUNMAP    __k_SYS_MUNMAP
#define SYS_MPROTECT  __k_SYS_MPROTECT
#define SYS_UNAME     __k_SYS_UNAME
#define SYS_GETTIME   __k_SYS_GETTIME
#define SYS_TEST      __k_SYS_TEST
#define SYS_FSTATAT   __k_SYS_FSTATAT
#define SYS_FCHDIR    __k_SYS_FCHDIR
#define SYS_TCB_SET   __k_SYS_TCB_SET
#define SYS_RENAMEAT  __k_SYS_RENAMEAT
#define SYS_GETRESUID __k_SYS_GETRESUID
#define SYS_GETRESGID __k_SYS_GETRESGID
#define SYS_SETRESUID __k_SYS_SETRESUID
#define SYS_SETRESGID __k_SYS_SETRESGID
#define SYS_GETUMASK  __k_SYS_GETUMASK
#define SYS_SETUMASK  __k_SYS_SETUMASK
#define SYS_GETGROUPS __k_SYS_GETGROUPS
#define SYS_SETGROUPS __k_SYS_SETGROUPS
#define SYS_FTRUNCATE __k_SYS_FTRUNCATE
#define SYS_FCHMODAT  __k_SYS_FCHMODAT
#define SYS_FCHOWNAT  __k_SYS_FCHOWNAT

struct file;
struct hart_context;
struct vnode;

struct syscall {
    intptr_t (*function)(const uintptr_t* restrict args);
    const char* identifier;
    unsigned long number;
};

#define define_syscall(sc_function, sc_number)                                            \
    static const __used __section(".syscalls") struct syscall __syscall_##sc_function = { \
        .function = &(sc_function),                                                       \
        .identifier = #sc_function,                                                       \
        .number = (sc_number),                                                            \
    }

extern const struct syscall unsorted_syscalls_start[];
extern const struct syscall unsorted_syscalls_end[];

void dispatch_syscall(struct hart_context* restrict regs);

intptr_t syscall_with_fd(const uintptr_t* restrict args, intptr_t (*op)(struct file* restrict fp, const uintptr_t* restrict args));
int syscall_dirfd(int dirfd, const char* restrict path, struct vnode** restrict out) __nodiscard;
int syscall_resolve_at(int dirfd, const char* restrict path, unsigned flags, struct vnode** restrict out) __nodiscard;
