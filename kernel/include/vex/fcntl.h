#pragma once

#include <usr/vex/fcntl.h>

#define O_RDONLY __k_O_RDONLY
#define O_WRONLY __k_O_WRONLY
#define O_RDWR   __k_O_RDWR
#define O_CREAT  __k_O_CREAT
#define O_TRUNC  __k_O_TRUNC
#define O_APPEND __k_O_APPEND

#define F_OK __k_F_OK
#define X_OK __k_X_OK
#define W_OK __k_W_OK
#define R_OK __k_R_OK

#define AT_FDCWD            __k_AT_FDCWD
#define AT_SYMLINK_NOFOLLOW __k_AT_SYMLINK_NOFOLLOW
#define AT_REMOVEDIR        __k_AT_REMOVEDIR
#define AT_EMPTY_PATH       __k_AT_EMPTY_PATH
