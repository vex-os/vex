#pragma once

#include <usr/vex/syscall.h>
#include <vex/compiler.h>
#include <vex/types.h>

#define SYS_EXIT     __k_SYS_EXIT
#define SYS_FORK     __k_SYS_FORK
#define SYS_EXEC     __k_SYS_EXEC
#define SYS_WAITPID  __k_SYS_WAITPID
#define SYS_GETPID   __k_SYS_GETPID
#define SYS_GETPPID  __k_SYS_GETPPID
#define SYS_YIELD    __k_SYS_YIELD
#define SYS_OPEN     __k_SYS_OPEN
#define SYS_CLOSE    __k_SYS_CLOSE
#define SYS_READ     __k_SYS_READ
#define SYS_WRITE    __k_SYS_WRITE
#define SYS_SEEK     __k_SYS_SEEK
#define SYS_FSTAT    __k_SYS_FSTAT
#define SYS_UNLINK   __k_SYS_UNLINK
#define SYS_DUP      __k_SYS_DUP
#define SYS_DUP2     __k_SYS_DUP2
#define SYS_IOCTL    __k_SYS_IOCTL
#define SYS_MKDIR    __k_SYS_MKDIR
#define SYS_READDIR  __k_SYS_READDIR
#define SYS_GETCWD   __k_SYS_GETCWD
#define SYS_CHDIR    __k_SYS_CHDIR
#define SYS_MOUNT    __k_SYS_MOUNT
#define SYS_UMOUNT   __k_SYS_UMOUNT
#define SYS_SYNC     __k_SYS_SYNC
#define SYS_SYNCFS   __k_SYS_SYNCFS
#define SYS_STATVFS  __k_SYS_STATVFS
#define SYS_MMAP     __k_SYS_MMAP
#define SYS_MUNMAP   __k_SYS_MUNMAP
#define SYS_MPROTECT __k_SYS_MPROTECT
#define SYS_UNAME    __k_SYS_UNAME
#define SYS_GETTIME  __k_SYS_GETTIME
#define SYS_TEST     __k_SYS_TEST

struct file;
struct hart_context;

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
