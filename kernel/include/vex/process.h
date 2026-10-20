#pragma once

#include <vex/cred.h>
#include <vex/event.h>
#include <vex/limits.h>
#include <vex/mutex.h>
#include <vex/signal.h>
#include <vex/spinlock.h>

typedef struct file_s file_t;
typedef struct pending_signal_s pending_signal_t;
typedef struct thread_s thread_t;
typedef struct vmspace_s vmspace_t;
typedef struct vnode_s vnode_t;

#define INIT_PID 1

typedef struct process_s process_t;

struct process_s {
    pid_t pid;
    pid_t ppid;
    int32_t exit_code;

    uint32_t refcount;

    bool zombie;
    bool collected;
    bool exiting;
    uint8_t exit_signal;

    spinlock_t lock;

    vmspace_t* vmspace;

    thread_t* threads;
    thread_t* exec_thread;     // Waiting in exec for the other threads to go, under lock
    uint32_t num_threads;      // Until reaped, under lock
    uint32_t num_live_threads; // Until thread_exit, under lock

    process_t* children;
    process_t* sibling_next;
    process_t* sibling_prev;

    process_t* hash_next;

    event_t event;
    event_t child_exited;

    credentials_t* credentials;
    mutex_t credentials_lock;

    signal_actions_t* signal_actions;
    pending_signal_t* exit_note;
    pending_signal_t* adopt_note;
    sigset_t sig_pending;
    pending_signal_t* sig_queue;

    vnode_t* cwd;
    mode_t umask;

    spinlock_t fd_lock;
    uint32_t num_fds;
    uint32_t max_fds;
    file_t** fds;
};

pid_t process_alloc_id(void) __nodiscard;
process_t* process_create(process_t* restrict parent) __nodiscard;
process_t* process_locate(pid_t pid) __nodiscard;
void process_incref(process_t* process);
void process_decref(process_t* process);

void process_exit(process_t* process, int exit_code);
void process_kill(process_t* process, int sig);
void process_thread_exit(process_t* process, thread_t* thread);
int process_dethread(process_t* process, thread_t* thread);
bool process_has_thread(process_t* process, pid_t tid) __nodiscard;
void process_thread_reaped(process_t* process, thread_t* thread);
bool process_claim(process_t* process) __nodiscard;
void process_destroy(process_t* process);

credentials_t* process_get_cred(process_t* process) __nodiscard;
void process_set_cred(process_t* process, credentials_t* credentials);

vnode_t* process_get_cwd(process_t* process) __nodiscard;
void process_set_cwd(process_t* process, vnode_t* node);

file_t* process_fd_get(process_t* process, int fd) __nodiscard;
int process_fd_alloc(process_t* process, file_t* file) __nodiscard;
int process_fd_dup(process_t* process, int old_fd, int new_fd) __nodiscard;
int process_fd_release(process_t* process, int fd);
int process_fds_copy(process_t* restrict process, process_t* restrict source) __nodiscard;
