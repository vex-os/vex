#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

#define BINFMT_ARGS_MAX 32 // Of argv and of envp each

struct credentials;
struct file;
struct vmspace;

struct entry_point {
    uintptr_t address;
    uintptr_t ustack;
};

struct binfmt_ops {
    int (*probe)(struct file* restrict fp, const uint8_t* restrict header, size_t header_size);
    int (*setup)(struct vmspace* restrict vms, struct file* restrict fp, const char** restrict argv, const char** restrict envp,
        const struct credentials* restrict cred, struct entry_point* restrict out);
};

struct binfmt_private {
    struct binfmt* next;
};

struct binfmt {
    struct binfmt_private priv;
    const struct binfmt_ops* ops;
    const char* identifier;
};

int add_binfmt(struct binfmt* restrict fmt);
int remove_binfmt(struct binfmt* restrict fmt);

// The credentials are the ones the program will run with,
// which setuid and setgid bits may have made differ from the caller's
int load_binary(struct vmspace* restrict vms, struct file* restrict fp, const char** restrict argv, const char** restrict envp,
    const struct credentials* restrict cred, struct entry_point* restrict out);
