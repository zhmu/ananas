/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __SYS_IOCTL_H__
#define __SYS_IOCTL_H__

#include <sys/cdefs.h>

__BEGIN_DECLS

int ioctl(int fd, unsigned long request, ...);

__END_DECLS

#endif /* __SYS_IOCTL_H__ */
