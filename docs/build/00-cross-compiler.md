# Cross-compiler
#### Clang + LLD
1. Ensure you have the correct packages at hand.
	- For ArchLinux the packages are `clang` and `lld`.
	- For Ubuntu the packages are `clang` and `lld`.
	- For other systems/distros package names may vary.
3. Clang toolchain is the default for _a bunch of reasons_ so I recomment running make like usual, although it is possible to ensure that we use clang by passing `TOOLCHAIN=clang` as a command line parameter.

#### GCC cross-compiler
1. GCC (unlike Clang) is not a cross-compiler by design, so you will need to build the toolchain for the specific target architecture.
2. Ensure you have built the toolchain with the correct target triplet set in `Makefile.gcc`. Who said using GCC on a clang-oriented osdev project is going to be fun?
2. Run make with `TOOLCHAIN=gcc`
