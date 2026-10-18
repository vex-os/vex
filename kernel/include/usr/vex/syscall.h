#ifndef USR_VEX_SYSCALL_H
#define USR_VEX_SYSCALL_H

#define __k_SYS_EXIT      1
#define __k_SYS_FORK      2
#define __k_SYS_EXEC      3
#define __k_SYS_WAITPID   4
#define __k_SYS_GETPID    5
#define __k_SYS_GETPPID   6
#define __k_SYS_YIELD     7 /* forwards to sched_yield */
#define __k_SYS_OPENAT    8
#define __k_SYS_CLOSE     9
#define __k_SYS_READ      10
#define __k_SYS_WRITE     11
#define __k_SYS_SEEK      12
#define __k_SYS_FSTAT     13
#define __k_SYS_UNLINKAT  14
#define __k_SYS_DUP       15
#define __k_SYS_DUP2      16
#define __k_SYS_IOCTL     17
#define __k_SYS_MKDIRAT   18
#define __k_SYS_READDIR   19
#define __k_SYS_GETCWD    20
#define __k_SYS_CHDIR     21
#define __k_SYS_MOUNT     22
#define __k_SYS_UMOUNT    23
#define __k_SYS_SYNC      24
#define __k_SYS_SYNCFS    25
#define __k_SYS_STATVFS   26
#define __k_SYS_MMAP      27
#define __k_SYS_MUNMAP    28
#define __k_SYS_MPROTECT  29
#define __k_SYS_UNAME     30
#define __k_SYS_GETTIME   31
#define __k_SYS_TEST      32 /* for testing purposes */
#define __k_SYS_FSTATAT   33
#define __k_SYS_FCHDIR    34
#define __k_SYS_TCB_SET   35
#define __k_SYS_RENAMEAT  36
#define __k_SYS_GETRESUID 37
#define __k_SYS_GETRESGID 38
#define __k_SYS_SETRESUID 39
#define __k_SYS_SETRESGID 40
#define __k_SYS_GETUMASK  41
#define __k_SYS_SETUMASK  42
#define __k_SYS_GETGROUPS 43
#define __k_SYS_SETGROUPS 44
#define __k_SYS_FTRUNCATE 45
#define __k_SYS_FCHMODAT  46
#define __k_SYS_FCHOWNAT  47
#define __k_SYS_CAPGET    48
#define __k_SYS_CAPSET    49

#endif /* USR_VEX_SYSCALL_H */
