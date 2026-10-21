#include <vex/errno.h>
#include <vex/firmware.h>
#include <vex/kmalloc.h>
#include <vex/spinlock.h>

typedef struct claim_s claim_t;

struct claim_s {
    claim_t* next;
    void* handle;
};

static spinlock_t claim_lock = SPINLOCK_INIT;
static claim_t* claim_head = nullptr;

static bool is_claimed_nolock(const firmware_node_t* restrict node)
{
    for(const claim_t* claim = claim_head; claim; claim = claim->next) {
        if(claim->handle == node->handle) {
            return true;
        }
    }

    return false;
}

int firmware_node_claim(const firmware_node_t* restrict node)
{
    claim_t* claim = kmalloc(sizeof(claim_t));

    if(claim == nullptr) {
        return -ENOMEM;
    }

    claim->handle = node->handle;

    spinlock_acquire(&claim_lock);

    if(is_claimed_nolock(node)) {
        spinlock_release(&claim_lock);
        kfree(claim);
        return -EBUSY;
    }

    claim->next = claim_head;
    claim_head = claim;

    spinlock_release(&claim_lock);

    return 0;
}

bool firmware_node_is_claimed(const firmware_node_t* restrict node)
{
    spinlock_acquire(&claim_lock);
    bool claimed = is_claimed_nolock(node);
    spinlock_release(&claim_lock);

    return claimed;
}
