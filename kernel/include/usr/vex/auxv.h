#ifndef USR_VEX_AUXV_H
#define USR_VEX_AUXV_H

#define __k_AT_NULL         0 /* End of vector */
#define __k_AT_IGNORE       1
#define __k_AT_EXECFD       2 /* File descriptor of program */
#define __k_AT_PHDR         3 /* Program headers for program */
#define __k_AT_PHENT        4 /* Size of program header entry */
#define __k_AT_PHNUM        5 /* Number of program headers */
#define __k_AT_PAGESZ       6
#define __k_AT_BASE         7 /* Base address of interpreter */
#define __k_AT_FLAGS        8
#define __k_AT_ENTRY        9
#define __k_AT_NOTELF       10
#define __k_AT_UID          11 /* Real uid */
#define __k_AT_EUID         12
#define __k_AT_GID          13 /* Real gid */
#define __k_AT_EGID         14
#define __k_AT_CLKTCK       17 /* Frequency of times() */
#define __k_AT_SECURE       23 /* Set when effective ids differ from real ones */
#define __k_AT_SYSINFO      32
#define __k_AT_SYSINFO_EHDR 33

#endif /* USR_VEX_AUXV_H */
