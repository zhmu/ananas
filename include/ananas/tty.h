/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

// XXX We need some structure for this
#define TIOCSCTTY 10000 // set control tty
#define TIOCSPGRP 10001 // set foreground process group
#define TIOCGPGRP 10002 // get foreground process group
#define TIOGDNAME 10003 // get device name

#define TIOCGETA  10004 // get terminal parameters
#define TIOCSETA  10005 // set terminal parameters immediatetly
#define TIOCSETW  10006 // set terminal parameters after output written
#define TIOCSETWF 10007 // set terminal parameters after output written - discard unread input

#define TIOCDRAIN 10008 // block until all output is written
#define TIOCSTOP  10009 // suspend output
#define TIOCSTART 10010 // restart suspended output

#define TIOCFLUSHR 10011 // flush read
#define TIOCFLUSHW 10012 // flush write
#define TIOCFLUSHRW 10013 // flush read+write

#define TIOCSBRK 10014 // set break
#define TIOCCBRK 10015 // clear break

