/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <signal.h>
#include <unistd.h>

int raise(int sig) { return kill(getpid(), sig); }
