#ifndef E9B2BC00_D1B5_467A_9CB9_047C40FE32F8
#define E9B2BC00_D1B5_467A_9CB9_047C40FE32F8
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>

struct thread;
struct pagemap;

struct process {
    struct pagemap* vm;
    struct thread* threads;
    intptr_t exit_code;
    pid_t pid;
};

#endif /* E9B2BC00_D1B5_467A_9CB9_047C40FE32F8 */
