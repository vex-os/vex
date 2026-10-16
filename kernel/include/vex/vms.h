#pragma once

#include <vex/compiler.h>
#include <vex/mutex.h>
#include <vex/types.h>

struct pagemap;

struct vm_region {
    struct vm_region* next; // Sorted by start
    uintptr_t start;        // Page aligned, inclusive
    uintptr_t end;          // Page aligned, exclusive
    unsigned vprot;         // Without VPROT_USER, which every user page has
};

struct vmspace {
    struct vm_region* regions;
    struct pagemap* pmap;
    struct mutex lock; // Covers regions and the user half of pmap
};

struct vmspace* vms_create(void) __nodiscard;
void vms_release(struct vmspace* restrict vms);

int vms_map_anon(struct vmspace* restrict vms, uintptr_t start, uintptr_t end, unsigned vprot);
int vms_protect(struct vmspace* restrict vms, uintptr_t start, uintptr_t end, unsigned vprot);
int vms_unmap_all(struct vmspace* restrict vms);

int vms_fault(struct vmspace* restrict vms, uintptr_t virt, unsigned access);
uintptr_t vms_touch(struct vmspace* restrict vms, uintptr_t virt) __nodiscard;

ssize_t vms_copyin(void* restrict dst, struct vmspace* restrict vms, uintptr_t src, size_t sz);
ssize_t vms_copyin_str(char* restrict dst, struct vmspace* restrict vms, uintptr_t src, size_t max_sz);
ssize_t vms_copyout(struct vmspace* restrict vms, uintptr_t dst, const void* restrict src, size_t sz);
ssize_t vms_copyout_str(struct vmspace* restrict vms, uintptr_t dst, const char* restrict src, size_t max_sz);
