#ifndef F7DF1819_15C7_467E_82C1_9D4D6A16B99D
#define F7DF1819_15C7_467E_82C1_9D4D6A16B99D
#pragma once

#include <sys/vfs_path.h>

struct dirent {
    char name[NAME_SIZE];
    unsigned nodetype;
};

#endif /* F7DF1819_15C7_467E_82C1_9D4D6A16B99D */
