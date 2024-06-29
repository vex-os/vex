## Building Vex
### Toolchain
To build Vex, the compiler toolchain must support GNU extensions.  
The utilities needed to build the kernel itself are:

1. C99 compiler with GCC-like command line parameters (eg. `clang`)  
2. Linker with GNU LD-like command line parameters (eg. `ld.lld`)  
3. GNU make (`gmake`) or a compatible makefile utility  

**NOTE:** the build system was only ever tested with Clang and LLVM toolchain; GNU toolchain is supported but was never tested.  


### MBI utilities
To produce a minimal bootable image (MBI) the host system also needs the following utilities installed:  

1. A POSIX-compliant `dd`  
2. GNU parted for image partitioning  
3. FAT32 `mkfs` (`mkfs.fat`) (`dosfstools`)  
4. GNU mtools (specifically `mmd` and `mcopy`)  


### Building the kernel
As simple as the following command:  
```
make kernel -j $(nproc)
```

To also produce an MBI run this:  
```
make bootable -j $(nproc)
```

**NOTE:** as of now (2024-06-29) the `all` target only depends on `kernel` and `bootable` phony targets, so running `make all` is equivalent to running `make bootable`. However, with the introduction of userspace and userspace utilities this might and will change.  

#### Setting targets
To set the target architecture prepend the command with this:  
```
ARCH=<architecture>
```

To set the toolchain prepend the command with this:  
```
TOOLCHAIN=<toolchain>
```
