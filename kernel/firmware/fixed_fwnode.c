#include <vex/errno.h>
#include <vex/fwnode.h>
#include <vex/sprintf.h>
#include <vex/string.h>

static const fwnode_ops_t fixed_node_ops;

static const fwnode_fixed_t* fixed_of(const fwnode_t* restrict node)
{
    return node->handle;
}

static int format(char* restrict buffer, size_t size, const char* restrict prefix, const char* restrict text)
{
    int formatted = snprintf(buffer, size, "%s%s", prefix, text);

    if(formatted < 0) {
        return -EIO;
    }

    if((size_t)(formatted) >= size) {
        return -ERANGE;
    }

    return 0;
}

static bool fixed_node_is_present(const fwnode_t* restrict node)
{
    return true;
}

static bool fixed_node_matches(const fwnode_t* restrict node, const char* const* restrict ids)
{
    const fwnode_fixed_t* fixed = fixed_of(node);

    for(const char* const* id = ids; *id; id += 1) {
        if(0 == strcmp(*id, fixed->id)) {
            return true;
        }
    }

    return false;
}

static int fixed_node_get_address(const fwnode_t* restrict node, uint64_t* restrict address)
{
    return -ENOENT;
}

static int fixed_node_get_cpu_id(const fwnode_t* restrict node, uint64_t* restrict cpu_id)
{
    return -ENOENT;
}

static int fixed_node_get_id(const fwnode_t* restrict node, char* restrict buffer, size_t size)
{
    const fwnode_fixed_t* fixed = fixed_of(node);

    return format(buffer, size, "", fixed->id);
}

static int fixed_node_get_name(const fwnode_t* restrict node, char* restrict buffer, size_t size)
{
    const fwnode_fixed_t* fixed = fixed_of(node);

    return format(buffer, size, "fixed:", fixed->name);
}

static void fixed_node_for_each_child(const fwnode_t* restrict node, void (*callback)(const fwnode_t* child, void* arg), void* arg)
{
    // empty
}

static int fixed_node_for_each_resource(const fwnode_t* restrict node, void (*callback)(const fwnode_resource_t* resource, void* arg),
    void* arg)
{
    const fwnode_fixed_t* fixed = fixed_of(node);

    if(fixed->num_resources == 0) {
        return -ENOENT;
    }

    for(size_t i = 0; i < fixed->num_resources; ++i) {
        callback(&fixed->resources[i], arg);
    }

    return 0;
}

static int fixed_node_route_pci_interrupt(const fwnode_t* restrict node, unsigned slot, unsigned pin, fwnode_irq_t* restrict irq)
{
    return -ENOENT;
}

static int fixed_node_for_each_dependency(const fwnode_t* restrict node, void (*callback)(const fwnode_t* provider, void* arg), void* arg)
{
    return -ENOENT;
}

static const fwnode_ops_t fixed_node_ops = {
    .is_present = &fixed_node_is_present,
    .matches = &fixed_node_matches,
    .get_address = &fixed_node_get_address,
    .get_cpu_id = &fixed_node_get_cpu_id,
    .get_id = &fixed_node_get_id,
    .get_name = &fixed_node_get_name,
    .for_each_child = &fixed_node_for_each_child,
    .for_each_resource = &fixed_node_for_each_resource,
    .route_pci_interrupt = &fixed_node_route_pci_interrupt,
    .for_each_dependency = &fixed_node_for_each_dependency,
};

void fwnode_init_fixed(fwnode_t* restrict node, const fwnode_fixed_t* restrict fixed)
{
    node->ops = &fixed_node_ops;
    node->handle = (void*)(fixed); // fixed_of adds const back
}
