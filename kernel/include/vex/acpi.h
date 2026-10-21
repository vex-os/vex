#pragma once

#include <vex/compiler.h>
#include <vex/io.h>
#include <vex/types.h>

struct acpi_gas;

/// Gets a region for the register a Generic Address Structure points to
/// @param length Of the register, in bytes
/// @retval -EINVAL The register is in an address space other than memory or ports
/// @retval -ENOMEM The memory couldn't be mapped
/// @note The memory stays mapped, as the registers are the system's own
int acpi_gas_region(const struct acpi_gas* restrict gas, size_t length, io_region_t* restrict region);
