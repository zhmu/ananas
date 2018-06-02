#ifndef __SYS_IOCTL_H__
#define __SYS_IOCTL_H__

#include <sys/cdefs.h>

__BEGIN_DECLS

int ioctl(int fd, unsigned long request, ...);

__END_DECLS

#endif /* __SYS_IOCTL_H__ */
