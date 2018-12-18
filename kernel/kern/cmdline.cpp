#include <ananas/types.h>
#include <ananas/bootinfo.h>
#include "kernel/cmdline.h"
#include "kernel/lib.h"
#include "kernel/vm.h"

static char* cmdline = NULL;
static int cmdline_len = -1;

void cmdline_init(char* bootargs, size_t bootargs_len)
{
    /* Default to no sensible commmandline */
    cmdline_len = 0;

    /* 1 because bi_args_size includes the terminating \0 */
    if (bootargs == nullptr || bootargs_len <= 1 || bootargs[bootargs_len - 1] != '\0')
        return;

    // Okay, looks sensible - claim them
    cmdline = bootargs;
    cmdline_len = bootargs_len;

    /*
     * Transform all spaces to \0's; this allows cmdline_get_string() to just
     * return on a match
     */
    for (int n = 0; n < cmdline_len; n++)
        if (cmdline[n] == ' ')
            cmdline[n] = '\0';
}

const char* cmdline_get_string(const char* key)
{
    KASSERT(cmdline_len >= 0, "cmdline not initialized");
    if (cmdline == NULL)
        return NULL;

    const char* cur = cmdline;
    while ((cur - cmdline) < cmdline_len) {
        /* Look for separator, = or \0 will do */
        const char* sep = cur;
        while (*sep != '\0' && *sep != '=')
            sep++;

        if (strncmp(cur, key, sep - cur) != 0) {
            /* No match; next one */
            cur = sep + 1;
            continue;
        }

        if (*sep == '=')
            sep++; /* skip the actual separator */
        return sep;
    }

    return NULL; /* not found */
}

/* vim:set ts=2 sw=2: */
