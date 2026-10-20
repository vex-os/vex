#pragma once

#include <usr/arch/signal.h>

#define MINSIGSTKSZ __k_MINSIGSTKSZ
#define SIGSTKSZ    __k_SIGSTKSZ

typedef struct __k_mcontext mcontext_t;
