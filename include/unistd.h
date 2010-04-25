#include <machine/_types.h>

#ifndef __UNISTD_H___
#define __UNISTD_H__

#define STDIN_FILENO	0
#define STDOUT_FILENO	1
#define STDERR_FILENO	2

ssize_t read(int fd, void* buf, size_t len);
ssize_t write(int fd, const void* buf, size_t len);
off_t	lseek(int fd, off_t offset, int whence);

#endif /* __UNISTD_H__ */
