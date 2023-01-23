# IO blocks
Some platforms and CPU architectures provide separate ways for the machine to "talk" with the devices attached to it both inside and outside. Two of these are the most common.

### Port-mapped IO
The ISA provides additional instructions for performing I/O operations (on x86 these are `out<b/w/l>` and `in<b/w/l>`), often through a separate bus.

### Memory-mapped IO
The devices are "sitting" on a specific space on the memory bus, allowing the CPU to access their registers without needing to have an additional instruction pack.

### Combining the two
* The `io_block_t` structure describes a block within a memory-mapped or port-mapped IO space that is designated to a specific device (let it be a serial port controller, an interrupt controller, a graphics card or whatever else).
```c
#include <kan/io.h>
typedef struct io_block_s {
	unsigned short mode;
	uintptr_t base;
	size_t size;
} io_block_t;
```
* The `mode` field of the structure defines how the system will treat read/write operations executed with this structure:
	- `IO_16_BIT` the address is 16-bit aligned and registers are 16-bit wide.
	- `IO_32_BIT` the address is 32-bit aligned and registers are 32-bit wide.
	- `IO_MEMORY_MAPPED` the IO block is memory-mapped.
	- `IO_PORT_MAPPED` the IO block is port-mapped.
	- `IO_PORT_MAPPED_SLOW` the port-mapped device requires some time "to think".
* The `base` field describes the first address of the block in range.
* The `size` field describes the amount of registers the block is allowed to read from and write to.
* If the register size (`IO_16_BIT` or `IO_32_BIT`) is not specified, a 8-bit register size and offset alignment is assumed.

### Functions
```c
#include <kan/io.h>
typedef struct io_block_s io_block_t;
int io_read(const io_block_t *restrict io, uintptr_t offset, uintmax_t *restrict out);
int io_write(const io_block_t *restrict io, uintptr_t offset, uintmax_t value);
```
* `io_read` performs a read operation, writes the result to `out`. Returns `EIO` if PMIO is not available, `EINVAL` if mode field doesn't have MMIO or PMIO bits, `ERANGE` if the offset is out of range.
* `io_write` performs a write operation. Returns `EIO` if PMIO is not available, `EINVAL` if mode field doesn't have MMIO or PMIO bits, `ERANGE` if the offset is out of range.


