/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/termios.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

#define TCSANOW 1
#define TCSADRAIN 2
#define TCSAFLUSH 3

int tcgetattr(int fildes, struct termios* termios_p);
int tcsetattr(int fildes, int optional_actions, const struct termios* termios_p);

speed_t cfgetispeed(const struct termios* termios_p);
speed_t cfgetospeed(const struct termios* termios_p);
int cfsetispeed(struct termios* termios_p, speed_t speed);
int cfsetospeed(struct termios* termios_p, speed_t speed);

#define TCOOFF 1
#define TCOON 2
#define TCIOFF 3
#define TCION 4

int tcflow(int fildes, int action);

#define TCIFLUSH 1
#define TCOFLUSH 2
#define TCIOFLUSH 3

int tcflush(int fildes, int queue_selector);

int tcsendbreak(int fildes, int duration);

__END_DECLS
