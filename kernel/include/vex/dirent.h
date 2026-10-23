#pragma once

#include <usr/vex/dirent.h>
#include <vex/path.h>
#include <vex/types.h>

#define DIR_DOTS_END 2 // The position after "." and ".."

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

/// Hands "." and ".." to the reader at the positions 0 and 1
/// @pre Called from vnode_ops_t.iterate, before any other entry
/// @param self The node number of the directory
/// @param parent The node number of its parent, or self for the root of a filesystem
/// @retval true Both entries are with the reader, and dir_context->position is at least DIR_DOTS_END
/// @retval false The reader is full; the caller stops
/// @note A filesystem that calls this starts its own positions at DIR_DOTS_END
bool dir_emit_dots(dir_context_t* restrict dir_context, ino_t self, ino_t parent);
