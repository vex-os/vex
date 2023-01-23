# Initcalls
The idea of completely getting rid of header files consisting of just `init` function was just too good of a bait to not bite it.  

The whole initcall system as an algorithm is derived from [Protura](https://github.com/mkilgore/protura).  

#### Declaring an initcall
An initcall is a function that returns `EOK` if the initialization of this exact subsystem was okay, `ENODEV` if the initialization didn't finish correctly but didn't fail either and any other value from `errno.h` that can tell the kind of error that had happened:
```c
#include <kan/initcall.h>
static int init_something(void)
{
	if(has_device) {
		init_device();
		return EOK;
	}

return ENODEV;
}
initcall_tier_0(something, init_something);
```

#### Initcall tiers
There are three initcall "hooks" provided by the header for other initcalls to depend on; essentially, these are "tiers" of initcalls:
1. Tier 0 is the earliest; subsystems here shall set up the memory management, logging and other things.
2. Tier 1 is the next; subsystems here shall initialize kind-of-portable aspects of the machine the system is running on.
3. Tier 2 is the last; subsystems here shall initialize drivers, load modules, mount the ramdisk and whatnot.

#### Initcall dependencies
Each initcall can depend on one or more initcalls. This means that an initcall _will not_ be called before _all of its dependencies have been called_.
