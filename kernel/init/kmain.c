#include <limine.h>
#include <vex/binfmt.h>
#include <vex/blk_device.h>
#include <vex/cmdline.h>
#include <vex/event.h>
#include <vex/file.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/mount.h>
#include <vex/panic.h>
#include <vex/process.h>
#include <vex/sched.h>
#include <vex/string.h>
#include <vex/version.h>

static volatile __used __section(".limine_requests") uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);
static volatile __used __section(".limine_requests_start") uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;
static volatile __used __section(".limine_requests_end") uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void mount_root(void)
{
    const char* device_name = cmdline_argument(&sys_cmdline, "root");

    if(device_name == nullptr) {
        panic("kmain: no root device specified");
        unreachable();
    }

    const struct blk_device* device = find_blk_device(device_name);

    if(device == nullptr) {
        panic("kmain: %s not a block device", device_name);
        unreachable();
    }

    int retval = vfs_mount(nullptr, "/", device, nullptr);

    if(retval < 0) {
        panic("kmain: %s: mount failed: %s", device_name, strerror(retval));
        unreachable();
    }
}

static void __noreturn init_watchdog(void* restrict arg)
{
    struct process* proc = arg;
    struct event* evnt = &proc->evnt;

    event_await_blocking(&evnt, 1);

    panic("kmain: init process died");
    unreachable();
}

static void spawn_init(void)
{
    const char* path = cmdline_argument(&sys_cmdline, "init");

    if(path == nullptr) {
        path = CONFIG_DEFAULT_INIT;
    }

    struct process* proc = process_create(nullptr);

    if(proc == nullptr) {
        panic("kmain: out of memory");
        unreachable();
    }

    struct file* fp;
    int retval = file_open(path, O_RDONLY, &fp);

    if(retval < 0) {
        panic("kmain: %s: file_open failed: %s", path, strerror(retval));
        unreachable();
    }

    const char* argv[] = { path, nullptr };
    const char* envp[] = { nullptr };

    struct entry_point entry;
    retval = load_binary(proc, fp, argv, envp, &entry);

    if(retval < 0) {
        panic("kmain: %s: load_binary failed: %s", path, strerror(retval));
        unreachable();
    }

    create_thread_user_enq(proc, entry.address, entry.ustack);
    create_thread_kern_enq(&init_watchdog, proc);
}

void __noreturn kmain(void)
{
    pr_emerg("%s %s %s", k_sysname, k_release, k_version);

    invoke_initcalls(initcalls_bsp_start, initcalls_bsp_end);

    pr_emerg("handing around...");

    mount_root();

    spawn_init();

    enable_interrupts();

    for(;;) {
        hart_spin();
    }

    unreachable();
}
