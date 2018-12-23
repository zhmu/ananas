/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <pthread.h>
#include <errno.h>

// XXX This is not thread-aware at all at this point
int pthread_once(pthread_once_t* once_control, void (*init_routine)(void))
{
    if (*once_control != PTHREAD_ONCE_INIT)
        return 0; // already done

    init_routine();
    (*once_control)++;
    return 0;
}
