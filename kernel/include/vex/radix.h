#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

struct radix_node;

struct radix {
    struct radix_node* root;
    unsigned height; // Levels from root to the items, zero when empty
};

void radix_init(struct radix* restrict tree);
void radix_deinit(struct radix* restrict tree);

void* radix_lookup(const struct radix* restrict tree, uint64_t index) __nodiscard;
int radix_insert(struct radix* restrict tree, uint64_t index, void* restrict item) __nodiscard;
void* radix_remove(struct radix* restrict tree, uint64_t index);

// Stores up to max items at or past start in index order; indices may be nullptr
size_t radix_gather(const struct radix* restrict tree, uint64_t start, void** restrict items, uint64_t* restrict indices,
    size_t max) __nodiscard;
