/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <unistd.h>
#include <ananas/tty.h>
#include <sys/ioctl.h>

pid_t tcgetpgrp(int fildes) { return ioctl(fildes, TIOCGPGRP); }
