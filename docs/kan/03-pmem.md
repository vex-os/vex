# Physical memory
To allow further progress, the kernel needs to allocate memory for itself, its processes and other internal things. The lowest level allocator is the physical page allocator.

### HHDM
According to the [bootloader protocol specification](https://github.com/limine-bootloader/limine/blob/v4.x-branch/PROTOCOL.md), HHDM (_Higher-Half Direct Map_) is virtual address that is specified by the bootloader. This address allows the kernel to access all the physical memory from a virtual higher-half address (that is in theory inaccessible for any of the CPUs due to their physical address limitations)

### Allocating
```c
#include <kan/pmem.h>
uintptr_t pmalloc(size_t psize);
void *pmalloc_hhdm(size_t psize);
void pmfree(uintptr_t pptr, size_t psize);
void pmfree_hhdm(void *restrict ptr, size_t psize);
```
* `pmalloc` allocates `psize` _contiguous_ pages (`PAGE_SIZE * psize` bytes) and returns a _physical_ page-aligned address to the memory block. If there is no free space left to fit the required amount of pages, it returns 0 (according to the protocol the very first physical page is ALWAYS inaccessible).
* `pmalloc_hhdm` is equivalent to `pmalloc` except it returns the page-aligned address with HHDM offset, ready for the internal kernel systems without needing to poll the offset themselves.
* `pmfree` de-allocates (marks as FREE) `psize` _contiguous_ pages at a specified _physical_ page-aligned address. **The function will mark any pages as FREE regardless of their current free/occupied state**
* `pmfree_hhdm` passes the address to `pmfree` after subtracting the HHDM offset from it.

### Utilities
The `<kan/pmem.h>` header also includes `<kan/pagedef.h>` header which contains self-documenting utility functions and platform-specific page definitions:
```c
#include <kan/pagedef.h>
#define PAGE_SIZE <target-defined>
uintptr_t page_align_address(uintptr_t address);
void *page_align_ptr(void *restrict ptr);
const void *page_align_cptr(const void *restrict ptr);
size_t get_page_count(size_t n);
```
