#include <vex/binfmt.h>
#include <vex/elf.h>
#include <vex/errno.h>
#include <vex/file.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/page.h>
#include <vex/pmap.h>
#include <vex/process.h>
#include <vex/string.h>
#include <vex/vms.h>
#include <vex/vprot.h>

#ifndef ELF64_ARCH
#define ELF64_ARCH EM_NONE
#endif

#define ELF64_STACK_UVA  UINT64_C(0x0000700000000000)
#define ELF64_STACK_SIZE PAGE_SIZE
#define ELF64_ARGV_MAX   32

static bool ehdr_valid(const struct elf64_ehdr* restrict ehdr)
{
    bool result = true;
    result = result && ehdr->e_ident[0] == 0x7F;
    result = result && ehdr->e_ident[1] == 0x45; // ASCII E
    result = result && ehdr->e_ident[2] == 0x4C; // ASCII L
    result = result && ehdr->e_ident[3] == 0x46; // ASCII F
    result = result && ehdr->e_ident[4] == 0x02; // ELFCLASS64

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    result = result && ehdr->e_ident[5] == 0x01; // ELFDATA2LSB
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    result = result && ehdr->e_ident[5] == 0x02; // ELFDATA2MSB
#else
#error Unknown endianness
#endif

    result = result && ehdr->e_machine == ELF64_ARCH;
    result = result && ehdr->e_type == ET_EXEC;
    result = result && ehdr->e_phentsize == sizeof(struct elf64_phdr);
    result = result && ehdr->e_phnum > 0;

    return result;
}

static unsigned phdr_vprot(uint32_t p_flags)
{
    unsigned vprot = VPROT_USER;

    if(p_flags & PF_R)
        vprot |= VPROT_READ;
    if(p_flags & PF_W)
        vprot |= VPROT_WRITE;
    if(p_flags & PF_X)
        vprot |= VPROT_EXEC;

    return vprot;
}

static int load_segment(struct process* restrict proc, struct file* restrict fp, const struct elf64_phdr* restrict phdr)
{
    unsigned vprot = phdr_vprot(phdr->p_flags);

    uintptr_t seg_start = page_align(phdr->p_vaddr);
    uintptr_t seg_end = page_align_up(phdr->p_vaddr + phdr->p_memsz);

    for(uintptr_t page_uva = seg_start; page_uva < seg_end; page_uva += PAGE_SIZE) {
        unsigned existing_vprot = pmap_resolve_vprot(proc->vms->pmap, page_uva);

        if(existing_vprot) {
            unsigned merged = vprot | existing_vprot;

            if(merged != existing_vprot) {
                int retval = vms_patch(proc->vms, page_uva, merged);

                if(retval < 0) {
                    return retval;
                }
            }
        }
        else {
            int retval = vms_anon(proc->vms, page_uva, vprot);

            if(retval < 0) {
                return retval;
            }
        }

        uintptr_t phys = pmap_resolve_address(proc->vms->pmap, page_uva);
        uint8_t* kva = phys_to_virt(phys);

        uintptr_t file_lo = phdr->p_vaddr;
        uintptr_t file_hi = phdr->p_vaddr + phdr->p_filesz;
        uintptr_t copy_lo, copy_hi;

        if(page_uva > file_lo) {
            copy_lo = page_uva;
        }
        else {
            copy_lo = file_lo;
        }

        if((page_uva + PAGE_SIZE) < file_hi) {
            copy_hi = page_uva + PAGE_SIZE;
        }
        else {
            copy_hi = file_hi;
        }

        if(copy_lo < copy_hi) {
            size_t length = copy_hi - copy_lo;
            off_t offset = phdr->p_offset + copy_lo - phdr->p_vaddr;

            if(file_seek(fp, offset, SEEK_SET) < 0) {
                return -EIO;
            }

            if(file_read(fp, kva + copy_lo - page_uva, length) != (ssize_t)(length)) {
                return -EIO;
            }
        }
    }

    return 0;
}

static __always_inline bool ustack_reserve(uint8_t** restrict kva, uintptr_t* restrict uva, uintptr_t low_bound, size_t sz)
{
    if(uva[0] - low_bound >= sz) {
        kva[0] -= sz;
        uva[0] -= sz;
        return true;
    }

    return false;
}

static uintptr_t build_ustack(void* restrict top_kva, uintptr_t top_uva, size_t avail, const char** restrict argv,
    const char** restrict envp, const struct elf64_ehdr* restrict ehdr)
{
    int argc = 0;

    while(argv && argv[argc]) {
        if(argc >= ELF64_ARGV_MAX)
            return 0;
        argc += 1;
    }

    int envc = 0;

    while(envp && envp[envc]) {
        if(envc >= ELF64_ARGV_MAX)
            return 0;
        envc += 1;
    }

    uint8_t* kva = top_kva;
    uintptr_t uva = top_uva;
    uintptr_t low_bound = top_uva - avail;

    uintptr_t argv_uva[ELF64_ARGV_MAX];
    uintptr_t envp_uva[ELF64_ARGV_MAX];

    for(int i = argc - 1; i >= 0; --i) {
        size_t len = strlen(argv[i]) + 1;

        if(!ustack_reserve(&kva, &uva, low_bound, len)) {
            return 0;
        }

        memcpy(kva, argv[i], len);
        argv_uva[i] = uva;
    }

    for(int i = envc - 1; i >= 0; --i) {
        size_t len = strlen(envp[i]) + 1;

        if(!ustack_reserve(&kva, &uva, low_bound, len)) {
            return 0;
        }

        memcpy(kva, envp[i], len);
        envp_uva[i] = uva;
    }

    if(!ustack_reserve(&kva, &uva, low_bound, uva % sizeof(uintptr_t))) {
        return 0;
    }

    struct elf64_auxv auxv[] = {
        { AT_PHDR, ehdr->e_phoff },
        { AT_PHENT, ehdr->e_phentsize },
        { AT_PHNUM, ehdr->e_phnum },
        { AT_ENTRY, ehdr->e_entry },
        { AT_PAGESZ, PAGE_SIZE },
        { AT_NULL, 0 },
    };

    size_t auxv_bytes = sizeof(auxv);

    if(!ustack_reserve(&kva, &uva, low_bound, auxv_bytes)) {
        return 0;
    }

    memcpy(kva, auxv, auxv_bytes);

    size_t fixed_size = 0;
    fixed_size += sizeof(uintptr_t) * (envc + 1);
    fixed_size += sizeof(uintptr_t) * (argc + 1);
    fixed_size += sizeof(uintptr_t); // argc

    if((uva - fixed_size) % (2 * sizeof(uintptr_t)) != 0) {
        if(!ustack_reserve(&kva, &uva, low_bound, sizeof(uintptr_t))) {
            return 0;
        }
    }

    if(!ustack_reserve(&kva, &uva, low_bound, sizeof(uintptr_t))) {
        return 0;
    }

    ((uintptr_t*)(kva))[0] = 0;

    for(int i = envc - 1; i >= 0; --i) {
        if(!ustack_reserve(&kva, &uva, low_bound, sizeof(uintptr_t))) {
            return 0;
        }

        ((uintptr_t*)(kva))[0] = envp_uva[i];
    }

    if(!ustack_reserve(&kva, &uva, low_bound, sizeof(uintptr_t))) {
        return 0;
    }

    ((uintptr_t*)(kva))[0] = 0;

    for(int i = argc - 1; i >= 0; --i) {
        if(!ustack_reserve(&kva, &uva, low_bound, sizeof(uintptr_t))) {
            return 0;
        }

        ((uintptr_t*)(kva))[0] = argv_uva[i];
    }

    if(!ustack_reserve(&kva, &uva, low_bound, sizeof(uintptr_t))) {
        return 0;
    }

    ((uintptr_t*)(kva))[0] = (uintptr_t)(argc);

    return uva;
}

static int elf64_probe(struct file* restrict fp, const uint8_t* restrict header, size_t header_size)
{
    if(header_size < sizeof(struct elf64_ehdr))
        return -ENOEXEC;
    if(!ehdr_valid((const struct elf64_ehdr*)(header)))
        return -ENOEXEC;
    return 0;
}

static int elf64_setup(struct process* restrict proc, struct file* restrict fp, const char** restrict argv, const char** restrict envp,
    struct entry_point* restrict out)
{
    struct elf64_ehdr ehdr;

    if(file_seek(fp, 0, SEEK_SET) < 0) {
        return -EIO;
    }

    if(file_read(fp, &ehdr, sizeof(ehdr)) != (ssize_t)(sizeof(ehdr))) {
        return -EIO;
    }

    if(!ehdr_valid(&ehdr)) {
        return -ENOEXEC;
    }

    vms_unmap_all(proc->vms); // clear all existing mappings in case we're SYS_EXEC'ing

    int retval;

    for(uint16_t i = 0; i < ehdr.e_phnum; ++i) {
        struct elf64_phdr phdr;

        if(file_seek(fp, ehdr.e_phoff + i * ehdr.e_phentsize, SEEK_SET) < 0) {
            retval = -EIO;
            goto error;
        }

        if(file_read(fp, &phdr, sizeof(phdr)) != (ssize_t)(sizeof(phdr))) {
            retval = -EIO;
            goto error;
        }

        if(phdr.p_type == PT_INTERP || phdr.p_type == PT_DYNAMIC) {
            retval = -ENOSYS;
            goto error;
        }

        if(phdr.p_type != PT_LOAD) {
            continue;
        }

        if(phdr.p_filesz > phdr.p_memsz) {
            retval = -ENOEXEC;
            goto error;
        }

        if(phdr.p_vaddr > UINTPTR_MAX - phdr.p_memsz) {
            retval = -ENOEXEC;
            goto error;
        }

        retval = load_segment(proc, fp, &phdr);

        if(retval < 0) {
            goto error;
        }
    }

    if(vms_anon(proc->vms, ELF64_STACK_UVA, VPROT_URW) < 0) {
        retval = -ENOMEM;
        goto error;
    }

    uintptr_t stack_top_uva = ELF64_STACK_SIZE + ELF64_STACK_UVA;
    uintptr_t stack_top_kva_phys = ELF64_STACK_SIZE + pmap_resolve_address(proc->vms->pmap, ELF64_STACK_UVA);
    void* stack_top_kva = phys_to_virt(stack_top_kva_phys);

    uintptr_t entry_rsp = build_ustack(stack_top_kva, stack_top_uva, ELF64_STACK_SIZE, argv, envp, &ehdr);

    if(entry_rsp == 0) {
        retval = -E2BIG;
        goto error;
    }

    out->address = ehdr.e_entry;
    out->ustack = entry_rsp;

    return 0;

error:
    vms_unmap_all(proc->vms);
    return retval;
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
