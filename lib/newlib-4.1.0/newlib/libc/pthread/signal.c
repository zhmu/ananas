/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <pthread.h>
#include <signal.h>

int pthread_sigmask(int how, const sigset_t* set, sigset_t* oset)
{
    return sigprocmask(how, set, oset);
}
