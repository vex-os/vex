#ifndef BC09F6C2_A0F0_4B50_A47E_D87316FAC355
#define BC09F6C2_A0F0_4B50_A47E_D87316FAC355
#pragma once

#include <core/cdefs.h>

#define CHR_NAME_PREFIX_TTY   "tty"
#define CHR_NAME_PREFIX_PTY   "pty"
#define CHR_NAME_PREFIX_FBDEV "fb"

struct chr_name;
struct chr_name_private {
    struct chr_name* next;
    unsigned next_iterator;
};

struct chr_name {
    struct chr_name_private priv;
    char device_prefix[32];
};

extern struct chr_name* chr_name_tty;
extern struct chr_name* chr_name_pty;
extern struct chr_name* chr_name_fbdev;

extern struct chr_name* chr_name_query(const char* restrict prefix) __nodiscard;
extern unsigned chr_name_increment(struct chr_name* restrict name) __nodiscard;
extern unsigned chr_name_count(const struct chr_name* restrict name) __nodiscard;

#endif /* BC09F6C2_A0F0_4B50_A47E_D87316FAC355 */
