#include <vex/page.h>
#include <vex/pslab.h>
#include <vex/signal.h>
#include <vex/string.h>

static_assert(sizeof(struct sighand) <= PAGE_SIZE);

struct sighand* sighand_create(struct sighand* restrict parent)
{
    struct sighand* sighand = pslab_alloc_virt();

    if(sighand == nullptr) {
        return nullptr;
    }

    // SIG_DFL with no flags and an empty mask is all zeroes
    memset(sighand, 0, sizeof(struct sighand));

    if(parent) {
        bool intr_state = spinlock_acquire_intr(&parent->lock);
        memcpy(sighand->actions, parent->actions, sizeof(sighand->actions));
        spinlock_release_intr(&parent->lock, intr_state);
    }

    sighand->lock = SPINLOCK_INIT;

    return sighand;
}

void sighand_destroy(struct sighand* restrict sighand)
{
    pslab_free_virt(sighand);
}

void sighand_exec(struct sighand* restrict sighand)
{
    bool intr_state = spinlock_acquire_intr(&sighand->lock);

    for(int i = 0; i < NSIG; ++i) {
        struct k_sigaction* action = &sighand->actions[i];

        if(action->sa_handler == SIG_IGN) {
            action->sa_flags = 0;
            action->sa_mask = 0;
            continue;
        }

        memset(action, 0, sizeof(struct k_sigaction));
    }

    spinlock_release_intr(&sighand->lock, intr_state);
}
