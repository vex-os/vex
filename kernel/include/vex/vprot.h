#pragma once

#define VPROT_NONE  0x0000U // Empty MMU protection flags
#define VPROT_READ  0x0001U // Page can be read from
#define VPROT_WRITE 0x0002U // Page can be written to
#define VPROT_EXEC  0x0004U // Page can be executed
#define VPROT_USER  0x0008U // Page is accessible from user mode

#define VPROT_CACHE_MASK 0x0030U // Memory type of the page
#define VPROT_CACHE_WB   0x0000U // Write-back, for ordinary memory
#define VPROT_CACHE_WT   0x0010U // Write-through
#define VPROT_CACHE_UC   0x0020U // Uncached, for device registers
#define VPROT_CACHE_WC   0x0030U // Write-combining, for framebuffers

#define VPROT_RW  (VPROT_READ | VPROT_WRITE)              // Kernel read-write page
#define VPROT_RWX (VPROT_READ | VPROT_WRITE | VPROT_EXEC) // Kernel read-write-execute page
#define VPROT_RX  (VPROT_READ | VPROT_EXEC)               // Kernel read-execute page

#define VPROT_URW  (VPROT_READ | VPROT_WRITE | VPROT_USER)              // User read-write page
#define VPROT_URWX (VPROT_READ | VPROT_WRITE | VPROT_EXEC | VPROT_USER) // User read-write-execute page
#define VPROT_URX  (VPROT_READ | VPROT_EXEC | VPROT_USER)               // User read-execute page

#define VPROT_MMIO (VPROT_RW | VPROT_CACHE_UC) // Memory-mapped IO page
