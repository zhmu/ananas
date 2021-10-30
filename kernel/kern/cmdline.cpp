/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/cmdline.h"
#include "kernel/lib.h"
#include "kernel/vm.h"

static char* cmdline = nullptr;
static int cmdline_len = -1;

void cmdline_init(char* bootargs)
{
    /* Default to no sensible commmandline */
    cmdline_len = 0;

    /* 1 because bi_args_size includes the terminating \0 */
    if (bootargs == nullptr)
        return;

    // Okay, looks sensible - claim them
    cmdline = bootargs;
    cmdline_len = strlen(cmdline);

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
    if (cmdline == nullptr)
        return nullptr;

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

    return nullptr; /* not found */
}
