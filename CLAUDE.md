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

- The top-level `GNUmakefile` is an optional convenience wrapper; Meson and ninja remain the real build system. `make <target>` runs `meson setup` if `build.ninja` is missing, then forwards to `ninja -C build <target>` (`make` alone builds the default target). `BUILD_DIR` and `CROSS` (a basename in `cross/`, default `amd64-clang`) override the defaults. `make clean` skips setup when there is no build directory; `make distclean` removes the build directory, including `.config`. Never put build logic in the GNUmakefile; add it to `meson.build`.
- `scripts/run-qemu-amd64.sh` takes the extra args `slow` (icount), `verbose` (`-d cpu_reset,int -no-reboot`), or a number (log to `qemu_<N>.log` instead of `qemu.log`). Under WSL it calls `qemu-system-x86_64.exe` with Windows paths.
- Making the image needs `sfdisk`, `mkfs.fat`, mtools (`mmd`, `mcopy`) and `mke2fs` from e2fsprogs. The DATA partition is ext2 with a test tree from `boot/mkimage.sh`. Check it after a run with `e2fsck -fn "build/boot/vexsys.boot.img?offset=$((10240 * 512))"`.
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
- **Initcalls replace explicit init sequencing.** Subsystems register with `initcall_bsp_<stage>(fn, INIT_FIRST|EARLY|MIDDLE|LATE|LAST)` or `initcall_smp_<stage>(...)` (`kernel/include/vex/initcall.h`). They are placed in `.initcalls.bsp.*` / `.initcalls.smp.*` sections that the linker script `SORT`s. The stage order is roughly: early → arch → bootinfo → pmm → slab → mmu → heap → irq → timer → syscall → multicore → bus → vfs → dev → fs → cpudev → binfmt → services → late. `kmain()` (`kernel/init/kmain.c`) only runs the BSP initcalls, mounts `root=` from the kernel cmdline, loads `init=` (falling back to `CONFIG_DEFAULT_INIT`) as the first process, and idles.
- **Syscalls** register the same way: `define_syscall(sys_foo, SYS_FOO)` puts an entry in a linker section, and `kernel/proc/syscall.c` builds the dispatch table at the syscall stage. Syscall numbers live in `kernel/include/usr/vex/syscall.h`. The implementations are in `kernel/proc/sys_*.c`.
- **Header layout:**
  - `kernel/include/vex/`: kernel-internal API, included as `<vex/...>`.
  - `kernel/include/usr/vex/`: the userspace-visible ABI (errno, stat, fcntl, syscall numbers…), with `__k_`-prefixed names. The matching `vex/` header includes it and re-exports the plain names, structs as `_t` typedefs (`typedef struct __k_stat stat_t;`).
  - `kernel/arch/<arch>/include/arch/`: what the architecture provides to generic code (`<arch/pmap.h>`, `<arch/hart.h>`, …). Generic headers such as `vex/pmap.h` include these and declare the arch-neutral interface. Arch-private headers are named `arch/amd64.*.h`.
  - A subsystem split over several files keeps what they share in a local `<name>_internal.h` next to them (`kernel/device/device_internal.h`, `fs/fat/fat_internal.h`), included as `"<name>_internal.h"`; it is never included from outside its directory.
- **Generic vs arch code:** `kernel/arch/amd64/` mirrors the generic directories (`mm`, `proc`, `irq`, `time`, `firmware`, `bus`, `core`) and implements their arch-specific halves.
- **Device model:** `kernel/device/` holds one tree of `device_t` nodes, one per piece of hardware, changed only through `device_worker`. A bus is a node with `bus_ops_t` (`<vex/device_bus.h>`); each bus lives in `kernel/bus/<bus>/`. Firmware descriptions (`fwnode_t`) come from `kernel/firmware/<firmware>/`. Drivers declare themselves with `define_driver` (`<vex/driver.h>`), and probe returns a `DRIVER_SCORE_*`. What userspace sees of a node (block, char, serial, tty, fb) is an interface on top of it, in `dev/`.
- **Storage stack:** `dev/` has the block and char interface registries (`block_interface_t`, `char_interface_t`; a filesystem calls the block interface it sits on its `volume`), the partition-table parsers (GPT/MBR) and the ATA/ATAPI helpers. `drivers/` holds the hardware drivers, one directory per device class (`storage/ahci.c`, `display/limine.c`, `input/ps2kbd.c`); a driver that outgrows one file gets its own subdirectory there. Class cores such as the tty layer belong in `dev/`, not `drivers/`. A device that shows a kernel subsystem to userspace lives with that subsystem, not in `dev/`: `/dev/devctl` in `device/devctl.c`, `/dev/kmsg` in `core/kmsg.c`. `fs/` has the VFS (vnode, vcache, mount, path, file) plus the devfs, echfs and FAT backends. Device names look like `ada0n1p1` (used in `boot/limine.conf`).
- **Logging:** use `pr_emerg` … `pr_verbose` (`vex/kprintf.h`). The `loglevel=verbose` cmdline option enables verbose output. Use `panic()` for fatal errors and `kassert()` for assertions (gated by `CONFIG_KASSERT`).

## Conventions

- Headers under `include/usr/` are shared with userspace and must compile as ANSI C89/ISO C90: no `//` comments, `nullptr`, `bool`, `static_assert`, `restrict`, `inline`, designated initializers, or declarations mixed with code. Fixed-width types come from the `__k_`-prefixed typedefs. Only `usr/vex/types.h` touches `<stdint.h>`, and only behind its C23/C++ check.
- Kernel-private headers use only `#pragma once`. Public headers under `include/usr/` (the userspace ABI) instead use a classic path-derived guard, e.g. `usr/vex/errno.h` → `USR_VEX_ERRNO_H`, closed with `#endif /* USR_VEX_ERRNO_H */`. Never start these names with `_`. Keep the guard in sync if the header is renamed.
- C23 idioms are used throughout: `nullptr`, `static_assert`, `unreachable()`, `restrict` pointer params, and `{}` for empty initialization (not `{ 0 }`).
- Don't initialize a struct with its fields on one line; it's hard to read. Either list the designated fields one per line, with a comma after the last one so clang-format keeps them apart, or start from `{}` and assign the fields after it.
- Every kernel type is used through a `_t` typedef. Tags end in `_s`, `_u` or `_e` (`struct thread_s`, `union fat_entry_u`, `enum cpu_feature_e`):
  - Forward declarations of other files' types (`typedef struct vnode_s vnode_t;`) form one block right after the includes; C11 allows identical typedefs to repeat across headers.
  - The typedefs of the file's own types form a second block right before its first struct definition (after the `#define`s), in definition order. Structs then refer to each other through pointers in any order.
  - An enum's typedef goes right after its definition, since enums can't be forward-declared.
  - Don't make pointer typedefs.
- Name a variable or field after its type's stem (`thread_t* thread`, `file_t* file`), not an abbreviation such as `thrd` or `fp`. Use a role name such as `parent`, `curr` or `dir` when that says more. Prefer whole words to abbreviations in new names, except established terms (`vnode`, `pmap`, `pslab`, `pdma`, `hart`, `irq`).
- A function named `_get`, `_find` or `_locate` that returns a reference-counted object (`process_locate`, `block_interface_find`, `process_get_cwd`) hands the caller a new reference to drop. Only a function that breaks this rule needs a comment saying so.
- Public functions are named `<subsystem>_<verb>`: `block_interface_add_root`, `block_interface_find`, `block_interface_for_each`, not `add_block_interface_root`.
- Test a bitmask against zero explicitly: `if(0 == (flags & FLAG))`, not `if(!(flags & FLAG))`.
- Compare pointers to `nullptr` only for the null case (`if(ptr == nullptr)`); for non-null, use the pointer itself (`if(ptr)`). A call whose result only signals failure can be negated directly: `if(!pmentry_get(table, i, true))`.
- C23 still allows implicit conversions, and the code relies on them. Never write `!= 0`, `!= nullptr`, `0 !=` or `nullptr !=`: a value or pointer that is tested for non-zero is used by itself (`if(count)`, `bool taken = *prev;`). `== 0` and `== nullptr` are fine.
- Cast only where an implicit conversion doesn't do the job: where the compiler would warn (a narrowing or a change of signedness it flags), a pointer to or from an integer, a varargs argument. A narrowing that compiles without a warning, such as `uint32_t value = UINT64_C(42);`, needs no cast. Wrap the operand of a cast in parentheses: `(uint32_t)(address)`, `(void*)(fault)`.
- Don't dereference a call's result in place (`this_hart()->caps`, `pframe_of(phys)->flags`). Put the result in a named local first (`hart_local_t* hart = this_hart();`), then use its fields. `(*prev)->next` in list walks is a plain pointer dereference and is fine.
- Never use a standalone increment or decrement statement (`++x;`, `x++;`, `--x;`, `x--;`). Outside a `for` header, write `x += 1` or `x -= 1`. Inside a `for` header, a one-letter counter uses `++i` or `--i`, and a longer name uses `page += 1` or `level -= 1`.
- A `for` header has all three parts or none (`for(;;)`). A loop that doesn't fill all three is a `while`, with the step at the end of its body.
- Top-level order in a translation unit:
  1. includes;
  2. forward declarations (other files' typedefs, `extern` asm/linker symbols);
  3. compile-time constants (object-like `#define`s, anonymous `enum`s);
  4. the file's own typedef block;
  5. type definitions;
  6. read-only data that lands in `.rodata`, such as `static const char* const names[] = { … }`;
  7. static and global variables, in any order among themselves;
  8. static functions;
  9. public functions;
  10. closers: `init_*` functions with their `initcall_*`, and `sys_*` handlers with their `define_syscall`, each macro right after its function.

  Further details:
  - Static functions go in dependency order, callees first, so a static prototype is only needed for mutual recursion.
  - An ops or driver table (`static const vnode_ops_t foo_ops = { .read = &foo_read, … }`) sits right after the last function it points to.
  - Function-like macros and `static_assert`s stay next to what they belong to.
  - Headers follow the same order, except that all prototypes come before the `static __always_inline` bodies.
- Code should document itself through names. A comment is only worth having when it says something the code can't: why it is done this way and not the obvious way, an invariant (locking, ownership, units, range), or an outside requirement (a spec, the Linux ABI, a hardware quirk). Never restate what the code next to it does:
  - Before writing a comment, try to make it unnecessary: rename (`free_slots` instead of `slots // A set bit means free`), move the code into a function whose name says what it does, or turn the claim into a `static_assert` or `kassert`.
  - Don't describe a function's algorithm above it. Preconditions and ownership rules that every caller must respect stay at the declaration: in the header for public functions, at the definition for static ones. A guarantee that only some callers rely on goes at those call sites, as the reason the caller is correct.
  - A public function whose contract the signature can't express (a calling protocol, lock or interrupt state, reference ownership, return codes with distinct meanings, an obligation of an `arch_*` implementation) gets a `///` block above its prototype. Most functions get no such block. Inside one:
    - Prose is one short line saying what the function does, only where the name doesn't, or the steps of a calling protocol. No reasons in it.
    - `@pre` states what must hold at the call (locks held or not, interrupt state, who calls, references the caller holds); `@post` what holds afterwards when the prose doesn't say it.
    - `@param` only for a parameter whose meaning the name doesn't carry; `@retval` lists return values that mean different things, `@return` describes a single kind of result.
    - `@note` holds what an implementation or a caller must know that isn't a precondition: a reason (`takes no lock, as a panic mid-write writes again`) or an obligation (`vcache_remove the target once its last name is gone`). Keep it to a sentence.
    - Commands use `@`, not `\`. A command's text wraps onto plain `/// ` lines, not indented under it.
    - The clang cross file builds with `-Wdocumentation`, so a `@param` that drifts from the signature fails the build.
    - An op in an ops table follows the same rules: a block only when the op's contract isn't obvious, commands included, as hover shows it where the op is implemented. Clang doesn't check commands on struct fields, so a change to an op's signature updates its block by hand.
    - Every declaration gets its own complete block, even when that repeats a neighbour's: `///` attaches to the next declaration only, and hover tooltips show nothing else, so neither a comment above a group nor a "same as X" helps there.
  - Constants and fields get a comment only when it expands a spec name (FAT `BS_*`/`BPB_*`, AHCI registers, ELF), adds units or meaning the name lacks, or states an invariant. Format headers such as `vex/elf.h` keep their full constant tables for future ports, but drop comments that only repeat the name.
  - Write documentation comments and comments about behavior approximately in ASD-STE100 Simplified Technical English: short sentences, one statement per sentence, active voice, present tense, simple common words with one meaning each.
  - Use `//` (`#` in assembly and shell), start with a capital letter, and leave off the trailing period. Headers under `include/usr/` are the userspace ABI and must stay ANSI C89/ISO C90, so they use `/* */`.
  - A short note about a single line goes after it, as long as the line still fits the 140-column limit.
  - Wrap longer comments at clause boundaries, at roughly the width of the code they describe (not much shorter).
  - Tagged comments: `TODO:` is work nobody has planned yet, `UNDONE:` is work whose approach is known but not done, `FIXME:` is either of those where the current behavior is a bug, and `NOTE:` is a remark worth the reader's attention. Repeating NOTE raises the weight: `NOTENOTE:` marks something the surrounding code can't be understood without.
  - A function or scope left empty on purpose holds a single `// empty` (`/* empty */` in C89 code).
  - No commented-out code. The jokes and rants already in comments are part of the project's history; leave them alone.
- `contrib/` is vendored third-party code: Limine, uACPI, kconfiglib, and EDK2 firmware. Each component has a `README.vex` and an `update.sh`. Don't edit it in place; update it through the script.
- Commit messages use a `<area>: <lowercase summary>` prefix (e.g. `general: some important change`).
- Don't commit on your own. Leave changes in the working tree for review; commit only when explicitly told to, and only to the active branch.
