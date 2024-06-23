## Source Roadmap
| Directory | Description |  
| --------- | ----------- |  
| arch      | Architecture-dependent directories |  
| boot      | Bootloader binaries, ROM images for emulators, configuration files |  
| config    | Build environment configuration, linker scripts |  
| drivers   | Device drivers |  
| filesys   | Filesystem drivers |  
| include   | Architecture-independent headers |  
| kernel    | Main kernel sources |  
| libk      | Kernel C library subset |  
| scripts   | Utility shell scripts |  

### Architecture-dependent directories
| Directory | Description |  
| --------- | ----------- |  
| include   | Architecutre-dependent headers |  
| mach      | Architecture-dependent kernel sources |  

### Userspace headers
* Things inside `arch/${ARCH}/include/uapi` and inside `include/uapi` are to be copied into `/usr/include` on the target system installation.  
* Any identifier (except include guards) in the `uapi` header must be prepended with the `_k_` prefix to avoid global namespace trespassing in userspace:  
  ```c
  typedef unsigned int _k_mode_t;
  ```
* Internal kernel headers explicitly include `uapi` headers define types for normal internal use:  
  ```c
  typedef _k_mode_t mode_t;
  ```
