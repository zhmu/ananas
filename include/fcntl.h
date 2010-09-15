#include <sys/types.h>

#ifndef __FCNTL_H__
#define __FCNTL_H__

#ifndef __MODE_T_DEFINED
typedef uint16_t	mode_t;
#define __MODE_T_DEFINED
#endif

/* Standard flags */
#define O_CREAT		0x0001
#define O_RDONLY	0x0002
#define O_WRONLY	0x0004
#define O_RDWR		(O_RDONLY | O_WRONLY)
#define O_DIRECTORY	0x0008
#define O_NOFOLLOW	0x0010
#define O_APPEND	0x0020
#define O_TRUNC		0x0040
#define O_ACCMODE	(O_CREAT | O_RDONLY | O_WRONLY | O_DIRECTORY | O_NOFOLLOW | O_APPEND | O_TRUNC)

/* Unsupported flags */
#define O_EXEC		0x0000
#define O_SEARCH	0x0000
#define O_EXCL		0x0000
#define O_NOCTTY	0x0000
#define O_TTY_INIT	0x0000

#define F_DUPFD		0
#define F_GETFD		1
#define F_SETFD		2
#define F_GETFL		3
# define FD_CLOEXEC	1
#define F_SETFL		4
#define F_GETOWN	5
#define F_SETOWN	6

int creat(const char*, mode_t);
int open(const char*, int, ...);
int fcntl(int fildes, int cmd, ...);

#endif /* __FCNTL_H__ */
