# Initcalls
The idea of completely getting rid of header files consisting of just `init` function was just too good of a bait to not bite it.  

The whole initcall system as an algorithm is derived from [Protura](https://github.com/mkilgore/protura).  

### Declaring initcalls
```c
#include <kan/initcall.h>
initcall_bind(init, func);
initcall_extern(init);
initcall_depend(init, dep);
initcall_tier_0(init, func);
initcall_tier_1(init, func);
initcall_tier_2(init, func);
```

* `initcall_bind` macro _binds_ a specific function (no arguments, returns `int`) to an initcall name.
* `initcall_extern` declares (exposes) an initcall handle for other initcalls to depend on.
* `initcall_depend` declares a _dependency_. If an initcall depends on an another initcall, it will not be called before it, only after.
* `initcall_tier_0` declares a tier-0 initcall. Tier-0 initcalls usually initialize the machine-dependent and critical systems like physical memory, virtual memory, early-time logging, (on x86) GDT and IDT, etc.
* `initcall_tier_1` declares a tier-1 initcall. Tier-1 initcalls usually setup more common kernel systems like VFS (WIP).
* `initcall_tier_2` declares a tier-2 initcall. Tier-2 initcalls setup drivers, load modules, mount ramdisks and whatnot.

### Example
```c
#include <kan/errno.h>
#include <kan/initcall.h>
#include <an_important_subsystem.h>
static int init_something(void)
{
	if(something_present) {
		init_driver();
		return EOK;
	}
	
	return ENODEV;
}
initcall_tier_2(something, init_something);
initcall_depend(something, important_subsystem);
```

Initcalls return a positive `errno` value:
* `EOK` (zero) means the initcall was fully successful.
* `ENODEV` means the initcall wasn't successful but wasn't a failure either (ie. the device driver didn't found the device).
* Any other error value signals a failure and will result in kernel panic.

### Build process
1. The build system links (squashes) a relocatible kernel binary (combines all the object files into one, big, object file).
2. The build system calls a script (`scripts/gen_initcalls.sh`) that dumps values from a specific section, pulls initcall dependency strings (`initcall_depend` declares a string: `<dependency> <initcall_name>`), and `tsort`-s them, then generates a C source file with a NULL-terminated list of initcalls.
3. The generated source file is compiled, linked with the initcall-less squashed object and the final (ish) kernel executable is produced.
