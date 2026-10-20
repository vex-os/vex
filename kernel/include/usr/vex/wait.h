#ifndef USR_VEX_WAIT_H
#define USR_VEX_WAIT_H

/* The low seven bits hold the terminating signal, zero for a normal exit */

#define __k_W_EXITCODE(code, sig) (((int)((code) & 0xFF) << 8) | ((sig) & 0x7F))

#define __k_WEXITSTATUS(status) (((status) >> 8) & 0xFF)
#define __k_WTERMSIG(status)    ((status) & 0x7F)
#define __k_WIFEXITED(status)   (__k_WTERMSIG(status) == 0)
#define __k_WIFSIGNALED(status) (__k_WTERMSIG(status))

#endif /* USR_VEX_WAIT_H */
