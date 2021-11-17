/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <signal.h>
#include <errno.h>

static inline unsigned int signal_mask(int signo)
{
    if (signo < SIGHUP || signo >= _SIGLAST)
        return 0;
    return 1 << (signo - 1);
}

int sigemptyset(sigset_t* set)
{
    set->sig[0] = 0;
    return 0;
}

int sigfillset(sigset_t* set)
{
    int result = sigemptyset(set);
    for (unsigned int signo = 1; result == 0 && signo < NSIG; signo++)
        result = sigaddset(set, signo);
    return result;
}

int sigaddset(sigset_t* set, int signo)
{
    unsigned int mask = signal_mask(signo);
    if (mask == 0) {
        errno = EINVAL;
        return -1;
    }
    set->sig[0] |= mask;
    return 0;
}

int sigdelset(sigset_t* set, int signo)
{
    unsigned int mask = signal_mask(signo);
    if (mask == 0) {
        errno = EINVAL;
        return -1;
    }
    set->sig[0] &= ~mask;
    return 0;
}

int sigismember(const sigset_t* set, int signo)
{
    unsigned int mask = signal_mask(signo);
    if (mask == 0) {
        errno = EINVAL;
        return -1;
    }
    return (set->sig[0] & mask) ? 1 : 0;
}
