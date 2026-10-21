#pragma once

#include <arch/io.h>
#include <vex/assert.h>
#include <vex/compiler.h>
#include <vex/types.h>

enum {
    IO_SPACE_PORT,
    IO_SPACE_MEMORY,
};

typedef struct io_region_s io_region_t;

// Registers reached through port I/O or mapped memory alike, as the buses between
// the device and the CPU decide; a port region only exists where the CPU has ports
struct io_region_s {
    unsigned space; // IO_SPACE_*
    uintptr_t base; // The first port, or the kernel's virtual address of the first byte
    size_t length;
};

static __always_inline uint8_t io_read8(const io_region_t* restrict region, uintptr_t offset)
{
    if(region->space == IO_SPACE_PORT) {
        return port_read8(region->base + offset);
    }

    return *(volatile uint8_t*)(region->base + offset);
}

static __always_inline uint16_t io_read16(const io_region_t* restrict region, uintptr_t offset)
{
    if(region->space == IO_SPACE_PORT) {
        return port_read16(region->base + offset);
    }

    return *(volatile uint16_t*)(region->base + offset);
}

static __always_inline uint32_t io_read32(const io_region_t* restrict region, uintptr_t offset)
{
    if(region->space == IO_SPACE_PORT) {
        return port_read32(region->base + offset);
    }

    return *(volatile uint32_t*)(region->base + offset);
}

static __always_inline uint64_t io_read64(const io_region_t* restrict region, uintptr_t offset)
{
    kassert(region->space == IO_SPACE_MEMORY);

    return *(volatile uint64_t*)(region->base + offset);
}

static __always_inline void io_write8(const io_region_t* restrict region, uintptr_t offset, uint8_t value)
{
    if(region->space == IO_SPACE_PORT) {
        port_write8(region->base + offset, value);
        return;
    }

    *(volatile uint8_t*)(region->base + offset) = value;
}

static __always_inline void io_write16(const io_region_t* restrict region, uintptr_t offset, uint16_t value)
{
    if(region->space == IO_SPACE_PORT) {
        port_write16(region->base + offset, value);
        return;
    }

    *(volatile uint16_t*)(region->base + offset) = value;
}

static __always_inline void io_write32(const io_region_t* restrict region, uintptr_t offset, uint32_t value)
{
    if(region->space == IO_SPACE_PORT) {
        port_write32(region->base + offset, value);
        return;
    }

    *(volatile uint32_t*)(region->base + offset) = value;
}

static __always_inline void io_write64(const io_region_t* restrict region, uintptr_t offset, uint64_t value)
{
    kassert(region->space == IO_SPACE_MEMORY);

    *(volatile uint64_t*)(region->base + offset) = value;
}

// The string forms read or write the same register count times, as a data port is used
static __always_inline void io_read8s(const io_region_t* restrict region, uintptr_t offset, uint8_t* restrict data, size_t count)
{
    if(region->space == IO_SPACE_PORT) {
        port_read8s(region->base + offset, data, count);
        return;
    }

    for(size_t i = 0; i < count; ++i) {
        data[i] = *(volatile uint8_t*)(region->base + offset);
    }
}

static __always_inline void io_read16s(const io_region_t* restrict region, uintptr_t offset, uint16_t* restrict data, size_t count)
{
    if(region->space == IO_SPACE_PORT) {
        port_read16s(region->base + offset, data, count);
        return;
    }

    for(size_t i = 0; i < count; ++i) {
        data[i] = *(volatile uint16_t*)(region->base + offset);
    }
}

static __always_inline void io_read32s(const io_region_t* restrict region, uintptr_t offset, uint32_t* restrict data, size_t count)
{
    if(region->space == IO_SPACE_PORT) {
        port_read32s(region->base + offset, data, count);
        return;
    }

    for(size_t i = 0; i < count; ++i) {
        data[i] = *(volatile uint32_t*)(region->base + offset);
    }
}

static __always_inline void io_write8s(const io_region_t* restrict region, uintptr_t offset, const uint8_t* restrict data, size_t count)
{
    if(region->space == IO_SPACE_PORT) {
        port_write8s(region->base + offset, data, count);
        return;
    }

    for(size_t i = 0; i < count; ++i) {
        *(volatile uint8_t*)(region->base + offset) = data[i];
    }
}

static __always_inline void io_write16s(const io_region_t* restrict region, uintptr_t offset, const uint16_t* restrict data, size_t count)
{
    if(region->space == IO_SPACE_PORT) {
        port_write16s(region->base + offset, data, count);
        return;
    }

    for(size_t i = 0; i < count; ++i) {
        *(volatile uint16_t*)(region->base + offset) = data[i];
    }
}

static __always_inline void io_write32s(const io_region_t* restrict region, uintptr_t offset, const uint32_t* restrict data, size_t count)
{
    if(region->space == IO_SPACE_PORT) {
        port_write32s(region->base + offset, data, count);
        return;
    }

    for(size_t i = 0; i < count; ++i) {
        *(volatile uint32_t*)(region->base + offset) = data[i];
    }
}

// Reads size bytes from consecutive registers, such as a table the device exposes
static __always_inline void io_copy_from(const io_region_t* restrict region, uintptr_t offset, void* restrict buffer, size_t size)
{
    uint8_t* bytes = buffer;

    for(size_t i = 0; i < size; ++i) {
        bytes[i] = io_read8(region, offset + i);
    }
}
