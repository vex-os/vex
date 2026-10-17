#pragma once

#define VPROT_NONE  0x0000U
#define VPROT_READ  0x0001U
#define VPROT_WRITE 0x0002U
#define VPROT_EXEC  0x0004U
#define VPROT_USER  0x0008U

#define VPROT_CACHE_MASK 0x0030U // Memory type of the page
#define VPROT_CACHE_WB   0x0000U // Write-back, for ordinary memory
#define VPROT_CACHE_WT   0x0010U // Write-through
#define VPROT_CACHE_UC   0x0020U // Uncached, for device registers
#define VPROT_CACHE_WC   0x0030U // Write-combining, for framebuffers

#define VPROT_RW  (VPROT_READ | VPROT_WRITE)
#define VPROT_RWX (VPROT_READ | VPROT_WRITE | VPROT_EXEC)
#define VPROT_RX  (VPROT_READ | VPROT_EXEC)

#define VPROT_URW  (VPROT_READ | VPROT_WRITE | VPROT_USER)
#define VPROT_URWX (VPROT_READ | VPROT_WRITE | VPROT_EXEC | VPROT_USER)
#define VPROT_URX  (VPROT_READ | VPROT_EXEC | VPROT_USER)

#define VPROT_MMIO (VPROT_RW | VPROT_CACHE_UC)
