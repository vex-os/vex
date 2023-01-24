# Kernel logs
When there is some useful information about the internal workings of the kernel, it is worth printing it somewhere.  
Simply sending the information to a platform-specific hack port (like E9 on x86 within a Bochs/QEMU environment) may be enough; but the more kernel grows in complexity, the less this [KISS](## "Keep It Simple, Stupid") way of logging is viable.

### Log levels
The obvious choice would be to introduce log levels (and a verbosity level) to control what messages "get a ride" and what messages are ignored:
```c
#include <kan/kprintf>
extern short kp_verbosity;
void kputs(short severity, const char *restrict s);
void kprintf(short severity, const char *restrict fmt, ...);
```

### Keeping the messages
At some point in development, a question arised: "What if I want to print new messages in multiple places now?". There is two answers to that question:
1. Just register the "sink" and only print new messages.
2. Maintain a certain amount of previous messages and print them when registering the "sink" and print new messages there as well.

Kan went the second route:
```c
#include <kan/kprintf.h>
#define KP_MSG_SZ 0x200
#define KP_HISTORY_SZ 0x100
typedef char kp_msg[KP_MSG_SZ];
extern kp_msg_t kp_history[KP_HISTORY_SZ];
extern size_t kp_msg_count;
```

### Uhh... what's a "sink"?
A system console driver. That's kind of it. The whole point of the message history handles being public is to allow console subsystem to maintain consoles correctly.
```c
#include <kan/console.h>
typedef struct console_s console_t;
int register_console(console_t *restrict con);
void unregister_console(console_t *restrict con);
void console_puts(const char *restrict s);
```

The `console_puts` is called by internal `kprintf` code and writes a certain string to all the consoles that are registered so far.

### What next?
At least now I have some plans on extending the console subsystem to make unregistering a faster and easier process and implement an ability for consoles to replace each other (for instance, the boot-time Limine-provided terminal is going to be replaced with an in-house framebuffer-based console).
