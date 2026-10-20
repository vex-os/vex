#pragma once

#include <vex/compiler.h>
#include <vex/mutex.h>
#include <vex/types.h>

typedef struct pmap_s pmap_t;
typedef struct vnode_s vnode_t;

#define VMS_MMAP_TOP UINT64_C(0x0000600000000000) // Placement without a fixed address searches down from here
#define VMS_MMAP_MIN UINT64_C(0x0000000000010000) // Keeps null pointer dereferences faulting

typedef struct vm_backing_s vm_backing_t;
typedef struct vm_region_s vm_region_t;
typedef struct vmspace_s vmspace_t;

struct vm_backing_s {
    vnode_t* node;
    uint64_t file_page; // File page at the start of the range
    bool shared;
    bool may_write; // Whether a shared mapping can ever be made writable
};

struct vm_region_s {
    vm_region_t* next; // Sorted by start
    uintptr_t start;   // Page aligned, inclusive
    uintptr_t end;     // Page aligned, exclusive
    unsigned vprot;    // Without VPROT_USER, which every user page has
    vnode_t* node;     // Holds a reference; nullptr for anonymous memory
    uint64_t file_page;
    bool shared;
    bool may_write;
};

struct vmspace_s {
    vm_region_t* regions;
    pmap_t* pmap;
    mutex_t lock;
};

vmspace_t* vms_create(void) __nodiscard;
void vms_release(vmspace_t* restrict vmspace);

int vms_map(vmspace_t* restrict vmspace, uintptr_t start, uintptr_t end, unsigned vprot, const vm_backing_t* restrict backing);
int vms_map_replace(vmspace_t* restrict vmspace, uintptr_t start, uintptr_t end, unsigned vprot, const vm_backing_t* restrict backing);
int vms_map_any(vmspace_t* restrict vmspace, size_t size, unsigned vprot, const vm_backing_t* restrict backing, uintptr_t* restrict out);
int vms_unmap(vmspace_t* restrict vmspace, uintptr_t start, uintptr_t end);
int vms_sync(vmspace_t* restrict vmspace, uintptr_t start, uintptr_t end, bool wait);
int vms_protect(vmspace_t* restrict vmspace, uintptr_t start, uintptr_t end, unsigned vprot);
int vms_unmap_all(vmspace_t* restrict vmspace);
int vms_clone(vmspace_t* restrict vmspace, vmspace_t* restrict source);

int vms_fault(vmspace_t* restrict vmspace, uintptr_t virt, unsigned access);
uintptr_t vms_touch(vmspace_t* restrict vmspace, uintptr_t virt) __nodiscard;
uintptr_t vms_pin(vmspace_t* restrict vmspace, uintptr_t virt) __nodiscard;
void vms_unpin(uintptr_t phys);

ssize_t vms_copyin(void* restrict dst, vmspace_t* restrict vmspace, uintptr_t src, size_t size);
ssize_t vms_copyin_str(char* restrict dst, vmspace_t* restrict vmspace, uintptr_t src, size_t max_sz);
ssize_t vms_copyout(vmspace_t* restrict vmspace, uintptr_t dst, const void* restrict src, size_t size);
ssize_t vms_copyout_str(vmspace_t* restrict vmspace, uintptr_t dst, const char* restrict src, size_t max_sz);
