#include <vex/chr_name.h>
#include <vex/initcall.h>
#include <vex/malloc.h>
#include <vex/panic.h>
#include <vex/string.h>
#include <vex/strings.h>

static struct chr_name* chr_name_head = nullptr;

struct chr_name* chr_name_tty;
struct chr_name* chr_name_pty;
struct chr_name* chr_name_fbd;

struct chr_name* chr_name_query(const char* restrict prefix)
{
    struct chr_name* name;

    for(name = chr_name_head; name; name = name->priv.next) {
        if(strcmp(name->device_prefix, prefix) == 0) {
            return name;
        }
    }

    name = kmalloc(sizeof(struct chr_name));

    if(name == nullptr) {
        return nullptr;
    }

    kstrncpy(name->device_prefix, prefix, sizeof(name->device_prefix));

    name->priv.next = chr_name_head;
    name->priv.next_iterator = 0;
    chr_name_head = name;

    return name;
}

unsigned chr_name_increment(struct chr_name* restrict name)
{
    unsigned result = name->priv.next_iterator;
    name->priv.next_iterator += 1;
    return result;
}

unsigned chr_name_count(const struct chr_name* restrict name)
{
    return name->priv.next_iterator;
}

static void init_chr_name_predefined(void)
{
    chr_name_tty = chr_name_query(CHR_NAME_PREFIX_TTY);
    chr_name_pty = chr_name_query(CHR_NAME_PREFIX_PTY);
    chr_name_fbd = chr_name_query(CHR_NAME_PREFIX_FBD);

    bool oom = false;
    oom = oom || chr_name_tty == nullptr;
    oom = oom || chr_name_pty == nullptr;
    oom = oom || chr_name_fbd == nullptr;

    if(oom) {
        panic("chr_name: out of memory");
    }
}

initcall_bsp_bus(init_chr_name_predefined, INIT_LATE);
