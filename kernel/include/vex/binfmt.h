#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

typedef struct credentials_s credentials_t;
typedef struct file_s file_t;
typedef struct vmspace_s vmspace_t;

#define BINFMT_ARGS_MAX 32 // Of argv and of envp each

typedef struct binfmt_entry_s binfmt_entry_t;
typedef struct binfmt_ops_s binfmt_ops_t;
typedef struct binfmt_s binfmt_t;
typedef struct binfmt_private_s binfmt_private_t;

struct binfmt_entry_s {
    uintptr_t address;
    uintptr_t user_stack;
};

struct binfmt_ops_s {
    int (*probe)(file_t* restrict file, const uint8_t* restrict header, size_t header_size);
    int (*setup)(vmspace_t* restrict vmspace, file_t* restrict file, const char** restrict argv, const char** restrict envp,
        const credentials_t* restrict credentials, binfmt_entry_t* restrict out);
};

struct binfmt_private_s {
    binfmt_t* next;
};

struct binfmt_s {
    binfmt_private_t priv;
    const binfmt_ops_t* ops;
    const char* name;
};

int binfmt_add(binfmt_t* restrict binfmt);
int binfmt_remove(binfmt_t* restrict binfmt);

// The credentials are the ones the program will run with,
// which setuid and setgid bits may have made differ from the caller's
int load_binary(vmspace_t* restrict vmspace, file_t* restrict file, const char** restrict argv, const char** restrict envp,
    const credentials_t* restrict credentials, binfmt_entry_t* restrict out);
