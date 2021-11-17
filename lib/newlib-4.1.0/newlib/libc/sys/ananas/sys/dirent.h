#ifndef _SYS_DIRENT_H
#define _SYS_DIRENT_H

#include <ananas/types.h>

#define MAXNAMLEN 255

/* XXX The next defines do not really belong here */
#define DE_FLAG_DONE 1
#define DIRENT_BUFFER_SIZE 4096

#define __dirfd(dp) ((dp)->d_fd)

struct dirent {
    __ino_t d_ino;
    char d_name[MAXNAMLEN + 1];
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

#endif
