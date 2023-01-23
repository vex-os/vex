# Cross-compiler
## Clang + LLD
1. Ensure you have the correct packages at hand.
	- For ArchLinux the packages are `clang` and `lld`.
	- For Ubuntu the packages are `clang` and `lld`.
	- For other systems/distros package names may vary.
2. Open the file `conf/tools.mk` with your favourite editor and set the variables in there to the following:
```mk
CC := clang
LD := ld.lld
```
3. The next time you run `make`, the build system will detect the compiler and add respective options to `CFLAGS`, `CPPFLAGS` and `LDFLAGS`.

## GCC cross-compiler
1. GCC (unlike Clang) is not a cross-compiler by design, so you will need to build the toolchain for the specific target architecture.
2. Ensure the `TARGET` variable in `Makefile` is set to the correct one. Who said using GCC on a clang-oriented osdev project is going to be fun?
3. Make sure you have built the toolchain (you will need at least `gcc` and `ld`).
4. Open the file `conf/tools.mk` with your faviourite editor and set the variables in there to the following:
```mk
TUPLE ?= <your_target_tuple>
CC := $(TUPLE)-gcc
LD := $(TUPLE)-ld
```
4. The next time you run `make`, the build system will detect the compiler and add respective options to `CFLAGS`, `CPPFLAGS` and `LDFLAGS`.
