#pragma once

#include <vex/compiler.h>

#define CHR_NAME_PREFIX_TTY "tty"
#define CHR_NAME_PREFIX_PTY "pty"
#define CHR_NAME_PREFIX_FBD "fbd"

typedef struct chr_name_s chr_name_t;
typedef struct chr_name_private_s chr_name_private_t;

struct chr_name_private_s {
    chr_name_t* next;
    unsigned next_iterator;
};

struct chr_name_s {
    chr_name_private_t priv;
    char device_prefix[32];
};

extern chr_name_t* chr_name_tty;
extern chr_name_t* chr_name_pty;
extern chr_name_t* chr_name_fbd;

chr_name_t* chr_name_query(const char* restrict prefix) __nodiscard;
unsigned chr_name_increment(chr_name_t* restrict name) __nodiscard;
unsigned chr_name_count(const chr_name_t* restrict name) __nodiscard;
