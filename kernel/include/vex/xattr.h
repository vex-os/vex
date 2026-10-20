#pragma once

#include <usr/vex/xattr.h>
#include <vex/types.h>

#define XATTR_CREATE  __k_XATTR_CREATE
#define XATTR_REPLACE __k_XATTR_REPLACE

#define XATTR_NAME_MAX __k_XATTR_NAME_MAX
#define XATTR_SIZE_MAX __k_XATTR_SIZE_MAX
#define XATTR_LIST_MAX __k_XATTR_LIST_MAX

enum {
    XATTR_USER,
    XATTR_TRUSTED,
    XATTR_SECURITY,
    NUM_XATTR_SPACES,
};

#define XATTR_CAPS_NAME "capability" // Within XATTR_SECURITY

typedef struct xattr_entry_s xattr_entry_t;

struct xattr_entry_s {
    unsigned space;
    char name[XATTR_NAME_MAX + 1]; // Without the namespace prefix
};
