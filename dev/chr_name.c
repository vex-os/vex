#include <core/assert.h>
#include <dev/chr_name.h>
#include <init/initcall.h>
#include <lib/string.h>
#include <lib/strings.h>
#include <mm/malloc.h>

static struct chr_name* chr_name_head = nullptr;

struct chr_name* chr_name_tty;
struct chr_name* chr_name_pty;
struct chr_name* chr_name_fbdev;

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

define_initcall(chr_name_predefined, INITCALL_STAGE_BSP_BUS, INITCALL_ORDER_LATE)
{
    chr_name_tty = chr_name_query(CHR_NAME_PREFIX_TTY);
    chr_name_pty = chr_name_query(CHR_NAME_PREFIX_PTY);
    chr_name_fbdev = chr_name_query(CHR_NAME_PREFIX_FBDEV);

    strict_kassert_text(chr_name_tty, "out of memory");
    strict_kassert_text(chr_name_pty, "out of memory");
    strict_kassert_text(chr_name_fbdev, "out of memory");
}
