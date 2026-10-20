#include <vex/assert.h>
#include <vex/errno.h>
#include <vex/radix.h>
#include <vex/slab.h>

#define RADIX_SHIFT     6
#define RADIX_SLOTS     (1U << RADIX_SHIFT)
#define RADIX_MASK      (RADIX_SLOTS - 1U)
#define RADIX_MAX_DEPTH ((64 + RADIX_SHIFT - 1) / RADIX_SHIFT)

typedef struct radix_node_s radix_node_t;

struct radix_node_s {
    void* slots[RADIX_SLOTS];
    unsigned count;
};

static_assert(sizeof(radix_node_t) <= SLAB_MAX_SIZE);

static bool fits(unsigned height, uint64_t index)
{
    unsigned bits = height * RADIX_SHIFT;
    return bits >= 64 || (index >> bits) == 0;
}

static unsigned slot_of(unsigned level, uint64_t index)
{
    return (index >> ((level - 1) * RADIX_SHIFT)) & RADIX_MASK;
}

static void free_subtree(radix_node_t* restrict node, unsigned level)
{
    if(level > 1) {
        for(unsigned i = 0; i < RADIX_SLOTS; ++i) {
            if(node->slots[i]) {
                free_subtree(node->slots[i], level - 1);
            }
        }
    }

    slab_free(node);
}

static size_t gather_subtree(const radix_node_t* restrict node, unsigned level, uint64_t base, uint64_t start, void** restrict items,
    uint64_t* restrict indices, size_t max)
{
    unsigned span_bits = (level - 1) * RADIX_SHIFT;
    unsigned first = 0;

    if(start > base) {
        uint64_t slot = (start - base) >> span_bits;

        if(slot >= RADIX_SLOTS) {
            return 0;
        }

        first = slot;
    }

    size_t found = 0;

    for(unsigned i = first; i < RADIX_SLOTS && found < max; ++i) {
        if(node->slots[i] == nullptr) {
            continue;
        }

        uint64_t child_base = base + ((uint64_t)(i) << span_bits);

        if(level > 1) {
            if(indices) {
                found += gather_subtree(node->slots[i], level - 1, child_base, start, items + found, indices + found, max - found);
            }
            else {
                found += gather_subtree(node->slots[i], level - 1, child_base, start, items + found, nullptr, max - found);
            }

            continue;
        }

        items[found] = node->slots[i];

        if(indices) {
            indices[found] = child_base;
        }

        found += 1;
    }

    return found;
}

void radix_init(radix_tree_t* restrict tree)
{
    tree->root = nullptr;
    tree->height = 0;
}

void radix_deinit(radix_tree_t* restrict tree)
{
    if(tree->root) {
        free_subtree(tree->root, tree->height);
    }

    radix_init(tree);
}

void* radix_lookup(const radix_tree_t* restrict tree, uint64_t index)
{
    if(tree->root == nullptr || !fits(tree->height, index)) {
        return nullptr;
    }

    radix_node_t* node = tree->root;

    for(unsigned level = tree->height; level > 1; level -= 1) {
        node = node->slots[slot_of(level, index)];

        if(node == nullptr) {
            return nullptr;
        }
    }

    return node->slots[index & RADIX_MASK];
}

int radix_insert(radix_tree_t* restrict tree, uint64_t index, void* restrict item)
{
    kassert(item);

    if(tree->root == nullptr) {
        unsigned height = 1;

        while(!fits(height, index)) {
            height += 1;
        }

        tree->root = slab_alloc(sizeof(radix_node_t));

        if(tree->root == nullptr) {
            return -ENOMEM;
        }

        tree->height = height;
    }

    while(!fits(tree->height, index)) {
        radix_node_t* top = slab_alloc(sizeof(radix_node_t));

        if(top == nullptr) {
            return -ENOMEM;
        }

        top->slots[0] = tree->root;
        top->count = 1;
        tree->root = top;
        tree->height += 1;
    }

    radix_node_t* node = tree->root;

    for(unsigned level = tree->height; level > 1; level -= 1) {
        unsigned slot = slot_of(level, index);
        radix_node_t* child = node->slots[slot];

        // A child left empty by a failed insert
        // is reused here or freed with the tree
        if(child == nullptr) {
            child = slab_alloc(sizeof(radix_node_t));

            if(child == nullptr) {
                return -ENOMEM;
            }

            node->slots[slot] = child;
            node->count += 1;
        }

        node = child;
    }

    unsigned slot = index & RADIX_MASK;

    if(node->slots[slot]) {
        return -EEXIST;
    }

    node->slots[slot] = item;
    node->count += 1;

    return 0;
}

void* radix_remove(radix_tree_t* restrict tree, uint64_t index)
{
    if(tree->root == nullptr || !fits(tree->height, index)) {
        return nullptr;
    }

    radix_node_t* path[RADIX_MAX_DEPTH];
    radix_node_t* node = tree->root;
    unsigned depth = 0;

    for(unsigned level = tree->height; level > 1; level -= 1) {
        path[depth] = node;
        depth += 1;

        node = node->slots[slot_of(level, index)];

        if(node == nullptr) {
            return nullptr;
        }
    }

    void* item = node->slots[index & RADIX_MASK];

    if(item == nullptr) {
        return nullptr;
    }

    node->slots[index & RADIX_MASK] = nullptr;
    node->count -= 1;

    // Empty nodes are unlinked bottom-up, the parent
    // of path[depth] sits at path[depth - 1]
    unsigned level = 1;

    while(node->count == 0 && depth > 0) {
        slab_free(node);

        depth -= 1;
        level += 1;
        node = path[depth];
        node->slots[slot_of(level, index)] = nullptr;
        node->count -= 1;
    }

    if(tree->root->count == 0) {
        slab_free(tree->root);
        radix_init(tree);
        return item;
    }

    while(tree->height > 1 && tree->root->count == 1 && tree->root->slots[0]) {
        radix_node_t* top = tree->root;
        tree->root = top->slots[0];
        tree->height -= 1;
        slab_free(top);
    }

    return item;
}

size_t radix_gather(const radix_tree_t* restrict tree, uint64_t start, void** restrict items, uint64_t* restrict indices, size_t max)
{
    if(tree->root == nullptr || max == 0 || !fits(tree->height, start)) {
        return 0;
    }

    return gather_subtree(tree->root, tree->height, 0, start, items, indices, max);
}
