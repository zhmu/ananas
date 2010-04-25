#ifndef __FCNTL_H__
#define __FCNTL_H__

typedef int mode_t;

/* Standard flags */
#define O_CREAT		0x0001
#define O_RDONLY	0x0002
#define O_WRONLY	0x0004
#define O_RDWR		(O_RDONLY | O_WRONLY)
#define O_DIRECTORY	0x0008
#define O_NOFOLLOW	0x0010
#define O_APPEND	0x0020
#define O_TRUNC		0x0040

/* Unsupported flags */
#define O_EXEC		0x0000
#define O_SEARCH	0x0000
#define O_EXCL		0x0000
#define O_NOCTTY	0x0000
#define O_TTY_INIT	0x0000


int creat(const char*, mode_t);
int open(const char*, int, ...);

#endif /* __FCNTL_H__ */
