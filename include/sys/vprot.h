#ifndef AB66BCFE_D7D1_4116_8D38_EE7F447F0FD4
#define AB66BCFE_D7D1_4116_8D38_EE7F447F0FD4
#pragma once

#define VPROT_NONE    0x0000U ///< Empty VMM protection flags
#define VPROT_READ    0x0001U ///< Page can be read from
#define VPROT_WRITE   0x0002U ///< Page can be written to
#define VPROT_EXEC    0x0004U ///< Page can be executed
#define VPROT_USER    0x0008U ///< Page is accessible from user mode
#define VPROT_NOCACHE 0x0010U ///< Page is not to be cached

#define VPROT_RW  (VPROT_READ | VPROT_WRITE)              ///< Kernel read-write page
#define VPROT_RWX (VPROT_READ | VPROT_WRITE | VPROT_EXEC) ///< Kernel read-write-execute page
#define VPROT_RX  (VPROT_READ | VPROT_EXEC)               ///< Kernel read-execute page

#define VPROT_URW  (VPROT_READ | VPROT_WRITE | VPROT_USER)              ///< User read-write page
#define VPROT_URWX (VPROT_READ | VPROT_WRITE | VPROT_EXEC | VPROT_USER) ///< User read-write-execute page
#define VPROT_URX  (VPROT_READ | VPROT_EXEC | VPROT_USER)               ///< User read-execute page

#define VPROT_MMIO (VPROT_RW | VPROT_NOCACHE) ///< Memory-mapped IO page

#endif /* AB66BCFE_D7D1_4116_8D38_EE7F447F0FD4 */
