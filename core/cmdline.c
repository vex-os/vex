#include <core/cmdline.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <ext/limine.h>
#include <init/initcall.h>
#include <lib/ctype.h>
#include <lib/string.h>
#include <lib/strings.h>
#include <mm/malloc.h>

struct cmdline sys_cmdline;

static volatile __used __section(".limine_requests") struct limine_executable_cmdline_request cmdline_request = {
    .id = LIMINE_EXECUTABLE_CMDLINE_REQUEST_ID,
    .revision = 0,
};

int parse_cmdline(struct cmdline* restrict cmdl, char* restrict buffer, size_t length)
{
    cmdl->buffer = nullptr;
    cmdl->length = 0;

    if(buffer == nullptr || length == 0) {
        goto invalid_args;
    }

    bool escape = false;

    for(size_t i = 0; i < length; ++i) {
        if(isspace(buffer[i]) && !escape) {
            buffer[i] = 0;
            continue;
        }

        if(buffer[i] == '\\' && !escape) {
            escape = true;
            continue;
        }

        escape = false;
    }

    cmdl->buffer = buffer;
    cmdl->length = length;
    return 0;

invalid_args:
    cmdl->buffer = nullptr;
    cmdl->length = 0;
    return -EINVAL;
}

bool cmdline_contains(const struct cmdline* restrict cmdl, const char* restrict option)
{
    if(cmdl->buffer) {
        size_t option_size = strlen(option);
        const char* token = cmdl->buffer;
        const char* end = cmdl->buffer + cmdl->length;

        while(token < end) {
            size_t token_size = strlen(token);

            if(token_size == option_size && 0 == strncasecmp(token, option, option_size)) {
                return true;
            }

            if(token_size > option_size && token[option_size] == '-' && 0 == strncasecmp(token, option, option_size)) {
                return true;
            }

            token += token_size + 1;

            while(token < end && *token == 0) {
                token += 1;
            }
        }
    }

    return false;
}

const char* cmdline_argument(const struct cmdline* restrict cmdl, const char* restrict option)
{
    if(cmdl->buffer) {
        size_t option_size = strlen(option);
        const char* token = cmdl->buffer;
        const char* end = cmdl->buffer + cmdl->length;

        while(token < end) {
            size_t token_size = strlen(token);

            if(token_size == option_size && 0 == strncasecmp(token, option, option_size)) {
                return &token[option_size];
            }

            if(token_size > option_size && token[option_size] == '=' && 0 == strncasecmp(token, option, option_size)) {
                return &token[option_size + 1];
            }

            token += token_size + 1;

            while(token < end && *token == 0) {
                token += 1;
            }
        }
    }

    return nullptr;
}

define_initcall(sys_cmdline_parse, INITCALL_STAGE_BSP_BOOTINFO, INITCALL_ORDER_MIDDLE)
{
    if(cmdline_request.response == nullptr) {
        sys_cmdline.buffer = nullptr;
        sys_cmdline.length = 0;
    }
    else {
        sys_cmdline.buffer = cmdline_request.response->cmdline;
        sys_cmdline.length = 1 + strlen(sys_cmdline.buffer);

        int retval = parse_cmdline(&sys_cmdline, sys_cmdline.buffer, sys_cmdline.length);

        if(retval < 0) {
            pr_warning("cmdline: parse error: %s", strerror(retval));
            sys_cmdline.buffer = nullptr;
            sys_cmdline.length = 0;
        }
    }
}

define_initcall(sys_cmdline_preserve, INITCALL_STAGE_BSP_HEAP, INITCALL_ORDER_LAST)
{
    if(sys_cmdline.buffer) {
        char* buffer_copy = kmalloc(sys_cmdline.length);

        if(buffer_copy == nullptr) {
            pr_warning("cmdline: out of memory");

            sys_cmdline.buffer = nullptr;
            sys_cmdline.length = 0;

            return;
        }

        memcpy(buffer_copy, sys_cmdline.buffer, sys_cmdline.length);

        sys_cmdline.buffer = buffer_copy;
    }
}
