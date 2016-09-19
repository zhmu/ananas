#ifndef __ANANAS_FLAGS_H__
#define __ANANAS_FLAGS_H__

/* Contains flags that are shared between kernel/usermode */

/* open() */
#define O_CREAT		(1 << 0)
#define O_RDONLY	(1 << 1)
#define O_WRONLY	(1 << 2)
#define O_RDWR		(1 << 3)
#define O_APPEND	(1 << 4)
#define O_EXCL		(1 << 5)
#define O_TRUNC		(1 << 6)
#define O_CLOEXEC	(1 << 7)
#define O_NONBLOCK	(1 << 8)

/* Internal use only */
#define O_DIRECTORY	(1 << 31)

/* lseek() */
#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2

/* fcntl() */
#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL	3
#define F_SETFL	4

#define FD_CLOEXEC 1

#endif /* __ANANAS_FLAGS_H__ */
