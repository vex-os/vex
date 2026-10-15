# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Vex is a hobby operating system kernel written in C (gnu2x) and built with Meson. It is booted by Limine over UEFI. amd64 is currently the only implemented architecture. The meson build maps riscv64/aarch64 names, and their EDK2/Limine binaries are vendored, but there is no `kernel/arch/` code for them yet. Userspace is being split out: `boot/utest/` holds a userspace test binary that the Meson build does **not** build yet.

## Build and run

Builds must be cross builds. Meson errors out without a cross file.

```sh
meson setup build --cross-file cross/amd64-clang.ini   # or cross/amd64-gcc.ini (x86_64-elf-* toolchain)
ninja -C build                    # builds build/kernel/vexsys.elf
ninja -C build image              # builds build/boot/vexsys.boot.img (GPT disk, FAT16 ESP + empty DATA partition)
ninja -C build qemu               # builds the image and boots it in QEMU with OVMF, debugcon on stdio
```

- `scripts/run-qemu-amd64.sh` takes the extra args `slow` (icount), `verbose` (`-d cpu_reset,int -no-reboot`), or a number (log to `qemu_<N>.log` instead of `qemu.log`). Under WSL it calls `qemu-system-x86_64.exe` with Windows paths.
- Making the image needs `sfdisk`, `mkfs.fat` and mtools (`mmd`, `mcopy`). It uses `echfs-utils` if it is installed.
- The defaults are `werror=true` and `warning_level=2`, so any new warning fails the build.
- There is no test suite or linter target. Format C code with `clang-format` (`.clang-format`: 4 spaces, 140 columns, `if(` with no space before the paren, `T* p`). `contrib/**` is excluded from formatting.

## Configuration (Kconfig)

Kconfiglib is vendored in `contrib/kconfiglib`. The config lives in `build/.config`. At configure time, `scripts/kconfig/sync_config.py` seeds it from `configs/<arch>_defconfig`, or from `-Ddefconfig=<file in configs/>`. Meson reconfigures whenever any Kconfig file changes.

- Targets: `ninja -C build menuconfig | oldconfig | listnewconfig | defconfig | savedefconfig`.
- To update a defconfig, run `ninja -C build savedefconfig` and copy `build/defconfig` over `configs/<arch>_defconfig`. Defconfigs only store values that differ from the Kconfig defaults.
- The generated `config.h` is force-included (`-include`) into every kernel translation unit. `CONFIG_*` macros are always visible, so don't add includes for them.
- Optional subsystems are gated in `meson.build` with `kconfig.get('CONFIG_X', 'n') == 'y'`, not with `#ifdef` around whole files. A new optional driver or filesystem needs a Kconfig entry and a matching gate in the directory's `meson.build`.

## Kernel architecture

- **Build/link:** all kernel sources (including vendored uACPI) are compiled into a static archive, then linked by calling `ld` directly with `kernel/arch/<arch>/kernel.ld`. Clang has no bare-metal driver for amd64. Each subdirectory's `meson.build` just appends to `kernel_sources`.
- **Initcalls replace explicit init sequencing.** Subsystems register with `initcall_bsp_<stage>(fn, INIT_FIRST|EARLY|MIDDLE|LATE|LAST)` or `initcall_smp_<stage>(...)` (`kernel/include/vex/initcall.h`). They are placed in `.initcalls.bsp.*` / `.initcalls.smp.*` sections that the linker script `SORT`s. The stage order is roughly: early → arch → bootinfo → pmm → slab → mmu → heap → ireq → timer → syscall → multicore → bus → vfs → dev → fs → cpudev → binfmt → services → late. `kmain()` (`kernel/init/kmain.c`) only runs the BSP initcalls, mounts `root=` from the kernel cmdline, loads `init=` (falling back to `CONFIG_DEFAULT_INIT`) as the first process, and idles.
- **Syscalls** register the same way: `define_syscall(sys_foo, SYS_FOO)` puts an entry in a linker section, and `kernel/proc/syscall.c` builds the dispatch table at the syscall stage. Syscall numbers live in `kernel/include/usr/vex/syscall.h`. The implementations are in `kernel/proc/sys_*.c`.
- **Header layout:**
  - `kernel/include/vex/`: kernel-internal API, included as `<vex/...>`.
  - `kernel/include/usr/vex/`: the userspace-visible ABI (errno, stat, fcntl, syscall numbers…), with `__k_`-prefixed names. The matching `vex/` header includes it and re-exports the plain names.
  - `kernel/arch/<arch>/include/arch/`: what the architecture provides to generic code (`<arch/pmap.h>`, `<arch/hart.h>`, …). Generic headers such as `vex/pmap.h` include these and declare the arch-neutral interface. Arch-private headers are named `arch/amd64.*.h`.
- **Generic vs arch code:** `kernel/arch/amd64/` mirrors the generic directories (`mm`, `proc`, `ireq`, `time`, `acpi`, `bus`, `core`) and implements their arch-specific halves.
- **Storage stack:** `dev/` has the block/char device registries, the partition-table parsers (GPT/MBR) and the ATA/ATAPI helpers. `drivers/` holds the hardware drivers (AHCI, PS/2 keyboard). `fs/` has the VFS (vnode, vcache, mount, path, file) plus the devfs, echfs and FAT backends. Device names look like `ada0n1p1` (used in `boot/limine.conf`).
- **Logging:** use `pr_emerg` … `pr_verbose` (`vex/kprintf.h`). The `loglevel=verbose` cmdline option enables verbose output. Use `panic()` for fatal errors and `kassert()` for assertions (gated by `CONFIG_KASSERT`).

## Conventions

- Kernel-private headers use only `#pragma once`. Public headers under `include/usr/` (the userspace ABI) instead use a classic path-derived guard, e.g. `usr/vex/errno.h` → `USR_VEX_ERRNO_H`, closed with `#endif /* USR_VEX_ERRNO_H */`. Never start these names with `_`. Keep the guard in sync if the header is renamed.
- C23 idioms are used throughout: `nullptr`, `static_assert`, `unreachable()`, `restrict` pointer params.
- Within a translation unit, `static` helpers come before the public functions; init functions sit at the end next to their `initcall_*`.
- Code should document itself through names. Comments are for a non-obvious "why" or an invariant that the code doesn't show, not for restating what it does:
  - Use `//` (`#` in assembly and shell), start with a capital letter, and leave off the trailing period.
  - A short note about a single line goes after it, as long as the line still fits the 140-column limit.
  - Wrap longer comments at clause boundaries, at roughly the width of the code they describe (not much shorter).
  - Don't document functions in headers unless the name is hard to understand and renaming isn't an option. Field comments are fine when they map a field to its spec name (FAT `BS_*`/`BPB_*`, ELF) or state an invariant such as ownership or locking rules.
- `contrib/` is vendored third-party code: Limine, uACPI, kconfiglib, and EDK2 firmware. Each component has a `README.vex` and an `update.sh`. Don't edit it in place; update it through the script.
- Commit messages use a `<area>: <lowercase summary>` prefix (e.g. `general: some important change`).
