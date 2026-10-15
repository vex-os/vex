#pragma once

#include <vex/event.h>
#include <vex/limits.h>
#include <vex/spinlock.h>

struct file;
struct thread;
struct vmspace;
struct vnode;

struct process {
    pid_t pid;
    pid_t ppid;
    int exit_code;

    spinlock_type lock;

    struct vmspace* vms;

    // Stack for each thread is allocated below the previous one
    // with an additional guard page; this points to the top of the last allocated stack
    uintptr_t thread_stack_top;

    struct thread* threads;
    unsigned long num_threads;

    struct process* children;
    struct process* cnext;
    struct process* cprev;

    struct process* pnext;

    struct event evnt;

    struct vnode* cwd;
    struct file** fds;
    size_t num_fds;
    size_t max_fds;
};

struct process* process_create(struct process* restrict parent) __nodiscard;
struct process* process_locate(pid_t pid) __nodiscard;
void process_exit(struct process* proc, int exit_code);
void process_destroy(struct process* proc);

struct file* dereference_fd(struct process* proc, int fd) __nodiscard;
int allocate_fd(struct process* proc, struct file* file) __nodiscard;
int duplicate_fd(struct process* proc, int old_fd, int new_fd) __nodiscard;
void release_fd(struct process* proc, int fd);
