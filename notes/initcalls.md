# Initcalls
Kaneru's initcall system takes a lot of
inspiration from a very similar system from
[Protura](https://github.com/mkilgore/Protura).  

Using initcalls for EVERYTHING including early
initialization allows the source code to contain
much less header garbage while making the porting
process much easier (generally everything that is
platform-related is initialized in the zero tier).  

The macros are used to populate a discardable section
of the kernel's executable, which is then extracted
and a dependency-sorted array of function pointers is
formed, which, in turn is iterated through inside the
kernel entry point.  

#### Tier 0 initcalls
Used to initialize essential components such as
page frame allocator, memory protection, interrupts
and other things required for the kernel to function.  

#### Tier 1 initcalls
Initializing most of portable subsystems and a little
bit of platform-specific code such as unique device
drivers. This step is most "action-packed" by the amount
of shit going on and capable of failing.  

#### Tier 2 initcalls
Initializing virtual filesystem, multitasking (SMP),
mounting the ramdisk and running /sbin/init from it.  
