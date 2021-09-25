/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef SYS_TTY_H
#define SYS_TTY_H

// XXX We need some structure for this
#define TIOCSCTTY 10000 // set control tty
#define TIOCSPGRP 10001 // set foreground process group
#define TIOCGPGRP 10002 // get foreground process group
#define TIOGDNAME 10003 // get device name

#endif // SYS_TTY_H
