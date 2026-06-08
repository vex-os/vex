#ifndef F4BD5FA3_37BB_4208_8E92_63D960F51831
#define F4BD5FA3_37BB_4208_8E92_63D960F51831
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>

struct pagemap;

struct uthread;
struct uprocess {
    struct uprocess* parent;
    struct uthread* threads;
    struct pagemap* vm;
    unsigned refcount;
    bool dying;
    pid_t id;
};

struct uprocess* uprocess_create(struct uprocess* restrict parent);
void uprocess_exit(struct uprocess* restrict proc);
void uprocess_unref(struct uprocess* restrict proc);

#endif /* F4BD5FA3_37BB_4208_8E92_63D960F51831 */
