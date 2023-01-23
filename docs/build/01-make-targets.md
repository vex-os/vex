# Make targets
## `all`
* _all_ is a [_standard_](https://www.gnu.org/software/make/manual/html_node/Standard-Targets.html) _phony_ target results in the build system compiling and generating everything that can be compiled and generated including bootable disk images (if applicable to the set `TARGET`).
* Minimally, _all_ depends on the _kernel_ target.

## `kernel`
* _kernel_ is a _phony_ target that results in the build system assembling/compiling/linking the kernel binary (`kan.elf`) and placing it in the source code tree root.
* The kernel binary is ignored by Git VCS (`.gitignore`) and is removed by _clean_ target.

## `clean`
* _clean_ is a _phony_ target that results in the build system deleting almost every single intermediate and finished build artifact (let it be object files, binaries or bootable images) except for built tool executables.
* The global variable `CLEAN` answers the "What to remove?" question. It is a list of files and subdirectories that need to be evaporated by the build system.

## `distclean`
* _distclean_ is a _phony_ target that results in the build system deleting _every single_ build artifact (object files, binaries and bootable images) _including_ built tool executables.
* The global variable `DISTCLEAN` alongside `CLEAN` answers the "What to remove?" question. It is a list of files and subdirectories that need to be evaporated by the build system.
* Essentially, _distclean_ brings the source code tree to the state it was cloned except for changes in non-ignored files.
