#pragma once

#include <vex/compiler.h>
#include <vex/pci.h>
#include <vex/types.h>

typedef struct pci_config_space_ops_s pci_config_space_ops_t;
typedef struct pci_config_space_private_s pci_config_space_private_t;
typedef struct pci_config_space_s pci_config_space_t;

struct pci_config_space_ops_s {
    uint8_t (*read8)(pci_config_space_t* restrict config_space, const pci_address_t* restrict address, uintptr_t offset);
    uint16_t (*read16)(pci_config_space_t* restrict config_space, const pci_address_t* restrict address, uintptr_t offset);
    uint32_t (*read32)(pci_config_space_t* restrict config_space, const pci_address_t* restrict address, uintptr_t offset);
    void (*write8)(pci_config_space_t* restrict config_space, const pci_address_t* restrict address, uintptr_t offset, uint8_t value);
    void (*write16)(pci_config_space_t* restrict config_space, const pci_address_t* restrict address, uintptr_t offset, uint16_t value);
    void (*write32)(pci_config_space_t* restrict config_space, const pci_address_t* restrict address, uintptr_t offset, uint32_t value);
};

struct pci_config_space_private_s {
    pci_config_space_t* next;
};

// A way to reach the configuration space of the buses bus_start through bus_end of a segment
struct pci_config_space_s {
    pci_config_space_private_t priv;
    const pci_config_space_ops_t* ops;
    const char* name;
    unsigned segment;
    unsigned bus_start;
    unsigned bus_end;
    void* ctxdata;
};

/// @retval -EEXIST Another way already reaches some of the buses
int pci_config_space_add(pci_config_space_t* restrict config_space);

/// @return The way to reach the bus, which stays for good, or nullptr when there is none
pci_config_space_t* pci_config_space_find(unsigned segment, unsigned bus) __nodiscard;

static __always_inline uint8_t pci_config_space_read8(pci_config_space_t* restrict config_space, const pci_address_t* restrict address,
    uintptr_t offset)
{
    return config_space->ops->read8(config_space, address, offset);
}

static __always_inline uint16_t pci_config_space_read16(pci_config_space_t* restrict config_space, const pci_address_t* restrict address,
    uintptr_t offset)
{
    return config_space->ops->read16(config_space, address, offset);
}

static __always_inline uint32_t pci_config_space_read32(pci_config_space_t* restrict config_space, const pci_address_t* restrict address,
    uintptr_t offset)
{
    return config_space->ops->read32(config_space, address, offset);
}

static __always_inline void pci_config_space_write8(pci_config_space_t* restrict config_space, const pci_address_t* restrict address,
    uintptr_t offset, uint8_t value)
{
    config_space->ops->write8(config_space, address, offset, value);
}

static __always_inline void pci_config_space_write16(pci_config_space_t* restrict config_space, const pci_address_t* restrict address,
    uintptr_t offset, uint16_t value)
{
    config_space->ops->write16(config_space, address, offset, value);
}

static __always_inline void pci_config_space_write32(pci_config_space_t* restrict config_space, const pci_address_t* restrict address,
    uintptr_t offset, uint32_t value)
{
    config_space->ops->write32(config_space, address, offset, value);
}
