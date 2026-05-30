|Directory|Description|
|----|----|
|`/arch/<target>/`|Arch-specific code goes here|
|`/arch/<target>/<target>/`|Private code (included inside arch) goes here|
|`/arch/<target>/arch/`|Public code (included by portable) goes here|
|`/boot/`|Stuff needed for debug bootable image|
|`/bus/`|Bus and "bus" drivers and APIs|
|`/conf/`|Build system configuration|
|`/core/`|Core kernel systems|
|`/dev/`|Device drivers, in future these would most certainly declare themselves inside devfs|
|`/fs/`|Filesystem drivers (eg. ext2)|
|`/doc/`|Where the documentation is|
|`/ext/`|Third-party code|
|`/init/`|Code related to kernel setup|
|`/lib/`|Kernel C library|
|`/mm/`|Memory management|
|`/pub/`|Stuff that would go into userspace as headers|
|`/sh/`|Scripts and whatnot|
|`/time/`|Time keeping, clocksrc and so on|
