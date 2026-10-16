#pragma once

#include <limine.h>
#include <vex/compiler.h>
#include <vex/types.h>

extern const struct limine_memmap_response* memmap;

bool memmap_is_ram(uintptr_t phys, size_t size) __nodiscard;
