/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <sys/mount.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STATFSBUF_INITIAL_LENGTH 16

static int statfsbuf_len = 0;
static int statfsbuf_entries = 0;
static struct statfs* statfsbuf = NULL;

int getmntinfo(struct statfs** mntbufp, int mode)
{
    FILE* f = fopen("/ankh/fs/mounts", "r");
    if (f == NULL) {
        errno = ENOENT;
        return -1;
    }

    if (!statfsbuf_len) {
        statfsbuf = malloc(STATFSBUF_INITIAL_LENGTH * sizeof(struct statfs));
        if (statfsbuf == NULL) {
            errno = ENOMEM;
            return -1;
        }
        statfsbuf_len = STATFSBUF_INITIAL_LENGTH;
    }

    /*
     * We only care about the first field of the mounts file as that
     * contains wherever the filesystem is mounted - we feed that into
     * statfs() to fill out the rest.
     */
    statfsbuf_entries = 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char* ptr = strchr(line, ' ');
        if (ptr == NULL)
            continue;
        *ptr = '\0';

        struct statfs* sfs = &statfsbuf[statfsbuf_entries];
        if (statfs(line, sfs) == 0)
            statfsbuf_entries++;

        // XXX handle resize if >STATFSBUF_INITIAL_LENGTH
    }

    fclose(f);
    *mntbufp = statfsbuf;
    return statfsbuf_entries;
}
