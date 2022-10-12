# Segmentation code for x86-64
The only actual purpose of setting up
segmentation in Long mode is to shut the
bootloader up.  

As far as I know, task switching is done
manually on almost every 64-bit architecture
and x86-64 doesn't seem to be an exception.  

Segmentation (GDT) initcall hook is still exposed
though for certain x86-related subsystems to
depend on it if they can't live without GDT set up
properly, such as Limine terminal.
