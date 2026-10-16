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

    // Held by the process table until process_destroy
    // by each thread until it is reaped and by each
    // process_locate caller until process_decref
    unsigned refcount;

    bool zombie;
    bool collected;

    spinlock_type lock; // Taken with interrupts disabled: the scheduler unlinks reaped threads

    struct vmspace* vms;

    struct thread* threads;
    unsigned long num_threads;

    // Linked under the parent's lock; orphans
    // move to init/PID1 when their parent dies
    struct process* children;
    struct process* cnext;
    struct process* cprev;

    struct process* pnext;

    struct event evnt;
    struct event child_exited;

    struct vnode* cwd;

    spinlock_type fd_lock;
    struct file** fds;
    size_t num_fds;
    size_t max_fds;
};

struct process* process_create(struct process* restrict parent) __nodiscard;
struct process* process_locate(pid_t pid) __nodiscard;
void process_incref(struct process* proc);
void process_decref(struct process* proc);

void process_exit(struct process* proc, int exit_code);
void process_thread_reaped(struct process* proc, struct thread* thrd);
bool process_claim(struct process* proc) __nodiscard;
void process_destroy(struct process* proc);

struct file* dereference_fd(struct process* proc, int fd) __nodiscard;
int allocate_fd(struct process* proc, struct file* file) __nodiscard;
int duplicate_fd(struct process* proc, int old_fd, int new_fd) __nodiscard;
int release_fd(struct process* proc, int fd);
