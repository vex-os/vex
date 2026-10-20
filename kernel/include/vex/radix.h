#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

typedef struct radix_node_s radix_node_t;

typedef struct radix_tree_s radix_tree_t;

struct radix_tree_s {
    radix_node_t* root;
    unsigned height; // Levels from root to the items, zero when empty
};

void radix_init(radix_tree_t* restrict tree);
void radix_deinit(radix_tree_t* restrict tree);

void* radix_lookup(const radix_tree_t* restrict tree, uint64_t index) __nodiscard;
int radix_insert(radix_tree_t* restrict tree, uint64_t index, void* restrict item) __nodiscard;
void* radix_remove(radix_tree_t* restrict tree, uint64_t index);

// Stores up to max items at or past start in index order; indices may be nullptr
size_t radix_gather(const radix_tree_t* restrict tree, uint64_t start, void** restrict items, uint64_t* restrict indices,
    size_t max) __nodiscard;
