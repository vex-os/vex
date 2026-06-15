#include <vex/process.h>

#define PROCTABLE_BUCKETS 1024

static struct process* proctable[PROCTABLE_BUCKETS];
static spinlock_type proctable_lock;
static pid_t next_pid = 1;

static __always_inline __nodiscard size_t pid_bucket(pid_t pid)
{
    return (size_t)(pid % PROCTABLE_BUCKETS);
}

struct process* process_create(struct process* restrict parent, struct pagemap* restrict vm, const char* restrict name)
{
}

struct process* process_locate(pid_t pid)
{
    size_t bucket = pid_bucket(pid);

    spinlock_acquire(&proctable_lock);

    for(struct process* it = proctable[bucket]; it; it = it->pnext) {
        if(it->pid == pid) {
            spinlock_release(&proctable_lock);
            return it;
        }
    }

    spinlock_release(&proctable_lock);

    return nullptr;
}

void process_destroy(struct process* proc)
{
}
