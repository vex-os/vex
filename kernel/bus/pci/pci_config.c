#include <vex/errno.h>
#include <vex/pci.h>
#include <vex/pci_config.h>
#include <vex/spinlock.h>

static spinlock_t config_lock = SPINLOCK_INIT;
static pci_config_space_t* config_head = nullptr;

static bool configs_overlap(const pci_config_space_t* restrict first, const pci_config_space_t* restrict second)
{
    return first->segment == second->segment && first->bus_start <= second->bus_end && second->bus_start <= first->bus_end;
}

int pci_config_space_add(pci_config_space_t* restrict config_space)
{
    spinlock_acquire(&config_lock);

    for(pci_config_space_t* it = config_head; it; it = it->priv.next) {
        if(it == config_space || configs_overlap(it, config_space)) {
            spinlock_release(&config_lock);
            return -EEXIST;
        }
    }

    config_space->priv.next = config_head;
    config_head = config_space;

    spinlock_release(&config_lock);

    return 0;
}

pci_config_space_t* pci_config_space_find(unsigned segment, unsigned bus)
{
    spinlock_acquire(&config_lock);

    for(pci_config_space_t* it = config_head; it; it = it->priv.next) {
        if(it->segment == segment && bus >= it->bus_start && bus <= it->bus_end) {
            spinlock_release(&config_lock);
            return it;
        }
    }

    spinlock_release(&config_lock);

    return nullptr;
}
