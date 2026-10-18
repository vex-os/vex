#pragma once

#include <vex/cred.h>
#include <vex/event.h>
#include <vex/limits.h>
#include <vex/mutex.h>
#include <vex/spinlock.h>

struct file;
struct thread;
struct vmspace;
struct vnode;

struct process {
    pid_t pid;
    pid_t ppid;
    int exit_code;

    unsigned refcount;

    bool zombie;
    bool collected;

    spinlock_type lock;

    struct vmspace* vms;

    struct thread* threads;
    unsigned long num_threads;

    struct process* children;
    struct process* cnext;
    struct process* cprev;

    struct process* pnext;

    struct event evnt;
    struct event child_exited;

    struct credentials* cred;
    struct mutex cred_lock;
    mode_t umask;

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

struct credentials* process_get_cred(struct process* proc) __nodiscard;
void process_set_cred(struct process* proc, struct credentials* cred);

struct vnode* process_get_cwd(struct process* proc) __nodiscard;
void process_set_cwd(struct process* proc, struct vnode* node);

struct file* dereference_fd(struct process* proc, int fd) __nodiscard;
int allocate_fd(struct process* proc, struct file* file) __nodiscard;
int duplicate_fd(struct process* proc, int old_fd, int new_fd) __nodiscard;
int release_fd(struct process* proc, int fd);
int copy_fds(struct process* restrict proc, struct process* restrict stem) __nodiscard;
