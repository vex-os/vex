#ifndef USR_VEX_MMAN_H
#define USR_VEX_MMAN_H

#define __k_PROT_NONE  0x0000
#define __k_PROT_READ  0x0001
#define __k_PROT_WRITE 0x0002
#define __k_PROT_EXEC  0x0004

#define __k_MAP_SHARED    0x0001
#define __k_MAP_PRIVATE   0x0002
#define __k_MAP_FIXED     0x0010
#define __k_MAP_ANONYMOUS 0x0020

#define __k_MS_ASYNC      0x0001
#define __k_MS_INVALIDATE 0x0002
#define __k_MS_SYNC       0x0004

#endif /* USR_VEX_MMAN_H */
