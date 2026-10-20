#pragma once

#include <vex/compiler.h>
#include <vex/limits.h>
#include <vex/types.h>

#define DEV_NAME_NONE UINT_MAX // An index or partition the name doesn't have

typedef struct dev_prefix_s dev_prefix_t;
typedef struct dev_unit_s dev_unit_t;
typedef struct dev_name_s dev_name_t;

struct dev_prefix_s {
    dev_unit_t* units; // Sorted by number
    const char* string;
    uint32_t hash; // The prefix field of a dev_t
};

struct dev_unit_s {
    dev_unit_t* prev;
    dev_unit_t* next;
    dev_prefix_t* prefix;
    unsigned number;
};

struct dev_name_s {
    dev_unit_t* unit;
    unsigned index;
    unsigned partition;
};

extern dev_prefix_t dev_prefix_ata;
extern dev_prefix_t dev_prefix_atapi;
extern dev_prefix_t dev_prefix_nvme;
extern dev_prefix_t dev_prefix_virtio;
extern dev_prefix_t dev_prefix_tty;
extern dev_prefix_t dev_prefix_pty;
extern dev_prefix_t dev_prefix_fbd;

/// Takes the lowest number under prefix that no unit has
/// @retval -ENOSPC Every number a dev_t can hold is taken
int dev_unit_alloc(dev_prefix_t* restrict prefix, dev_unit_t** restrict unit);

/// Takes the given number, such as one the hardware assigns
/// @retval -EEXIST Another unit has the number
/// @retval -ERANGE A dev_t can't hold the number
int dev_unit_claim(dev_prefix_t* restrict prefix, unsigned number, dev_unit_t** restrict unit);

/// @pre The devices named with the unit are removed
/// @note Their names and dev_t numbers stay valid, but not the unit in their dev_name_t
void dev_unit_free(dev_unit_t* restrict unit);

/// @retval -ERANGE A dev_t can't hold the index or partition
/// @retval -ENAMETOOLONG The buffer is too small
int dev_name_format(const dev_name_t* restrict name, char* restrict buffer, size_t size);

dev_t dev_name_to_dev(const dev_name_t* restrict name) __nodiscard;
