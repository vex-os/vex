#include <vex/binfmt.h>
#include <vex/elf.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/vfs_file.h>

#ifndef ELF64_ARCH
#define ELF64_ARCH EM_NONE
#endif

static int elf64_probe(struct file* restrict fp, const uint8_t* restrict header, size_t header_size)
{
    return -ENOEXEC;
}

static int elf64_setup(struct process* restrict proc, struct file* restrict fp, const char** restrict argv, const char** restrict envp,
    struct entry_point* restrict out)
{
    return -EINVAL;
}

static const struct binfmt_ops elf64_ops = {
    .probe = &elf64_probe,
    .setup = &elf64_setup,
};

static struct binfmt elf64_binfmt = {
    .ops = &elf64_ops,
    .identifier = "elf64",
};

static void init_elf64(void)
{
    add_binfmt(&elf64_binfmt);
}

initcall_bsp_binfmt(init_elf64, INIT_MIDDLE);
