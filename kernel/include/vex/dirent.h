#pragma once

#include <usr/vex/dirent.h>
#include <vex/path.h>
#include <vex/types.h>

typedef struct __k_dirent dirent_t;
typedef struct dir_context_s dir_context_t;

struct dir_context_s {
    uint64_t position;
    bool (*emit)(dir_context_t* restrict dir_context, const dirent_t* restrict entry);
    void* state;
};

/// Hands one entry to the reader
/// @pre Called from vnode_ops_t.iterate, with the entries in the order of their positions
/// @param next_position The position of the entry after this one
/// @retval true The reader took the entry, and dir_context->position is next_position
/// @retval false The reader is full; the caller stops and leaves dir_context->position as it is
bool dir_emit(dir_context_t* restrict dir_context, dirent_t* restrict entry, uint64_t next_position);
