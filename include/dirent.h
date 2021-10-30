/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __DIRENT_H__
#define __DIRENT_H__

#include <ananas/types.h>
#include <sys/cdefs.h>

#define DIRENT_BUFFER_SIZE 4096 /* XXX */

#define DE_FLAG_DONE 0x0001 /* Everything has been read */
#define MAXNAMELEN 255      /* Name cannot be longer than this */

struct dirent {
    __ino_t d_ino;
    char d_name[MAXNAMELEN + 1];
};

typedef struct {
    int d_fd;                  /* file descriptor used */
    int d_flags;               /* flags */
    void* d_buffer;            /* buffer for temporary storage */
    unsigned int d_cur_pos;    /* Current position */
    unsigned int d_buf_size;   /* Buffer size */
    unsigned int d_buf_filled; /* Amount of buffer in use */
    struct dirent d_entry;     /* Current dir entry */
} DIR;

__BEGIN_DECLS

int closedir(DIR* dirp);
int dirfd(DIR* dirp);
DIR* opendir(const char* filename);
struct dirent* readdir(DIR* dirp);
void rewinddir(DIR* dirp);
void seekdir(DIR* dirp, long loc);
long telldir(DIR* dirp);

__END_DECLS

#endif /* __DIRENT_H__ */
