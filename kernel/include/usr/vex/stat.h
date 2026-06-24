#ifndef C1265F82_E321_4C99_9CFE_1C35CA0EE261
#define C1265F82_E321_4C99_9CFE_1C35CA0EE261

#include <vex/types.h>

/* These constants must be the same as ext2 filesystem ones */

#define __k_S_IFBLK  ((__k_mode_t)(0x1000))
#define __k_S_IFCHR  ((__k_mode_t)(0x2000))
#define __k_S_IFIFO  ((__k_mode_t)(0x3000))
#define __k_S_IFREG  ((__k_mode_t)(0x4000))
#define __k_S_IFDIR  ((__k_mode_t)(0x5000))
#define __k_S_IFLNK  ((__k_mode_t)(0x6000))
#define __k_S_IFSOCK ((__k_mode_t)(0x7000))
#define __k_S_IFMT   ((__k_mode_t)(0xF000))

#define __k_S_IXOTH ((__k_mode_t)(0x0001))
#define __k_S_IWOTH ((__k_mode_t)(0x0002))
#define __k_S_IROTH ((__k_mode_t)(0x0004))

#define __k_S_IXGRP ((__k_mode_t)(0x0008))
#define __k_S_IWGRP ((__k_mode_t)(0x0010))
#define __k_S_IRGRP ((__k_mode_t)(0x0020))

#define __k_S_IXUSR ((__k_mode_t)(0x0040))
#define __k_S_IWUSR ((__k_mode_t)(0x0080))
#define __k_S_IRUSR ((__k_mode_t)(0x0100))

#define __k_S_IRWXO (__k_S_IROTH | __k_S_IWOTH | __k_S_IXOTH)
#define __k_S_IRWXG (__k_S_IRGRP | __k_S_IWGRP | __k_S_IXGRP)
#define __k_S_IRWXU (__k_S_IRUSR | __k_S_IWUSR | __k_S_IXUSR)

#define __k_S_ISVTX ((__k_mode_t)(0x0200))
#define __k_S_ISGID ((__k_mode_t)(0x0400))
#define __k_S_ISUID ((__k_mode_t)(0x0800))

struct __k_stat {
    __k_dev_t st_dev;
    __k_ino_t st_ino;
    __k_mode_t st_mode;
    __k_nlink_t st_nlink;
    __k_uid_t st_uid;
    __k_gid_t st_gid;
    __k_dev_t st_rdev;
    __k_off_t st_size;
    __k_time_t st_atime;
    __k_time_t st_mtime;
    __k_time_t st_ctime;
    __k_blksize_t st_blksize;
    __k_blkcnt_t st_blocks;
};

#endif /* C1265F82_E321_4C99_9CFE_1C35CA0EE261 */
