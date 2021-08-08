/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>

#define PTHREAD_ONCE_BUSY 1
#define PTHREAD_ONCE_DONE 2

// XXX This is not thread-aware at all at this point
int pthread_once(pthread_once_t* once_control, void (*init_routine)(void))
{
    while(1) {
        int old_val = __sync_val_compare_and_swap(once_control, PTHREAD_ONCE_INIT, PTHREAD_ONCE_BUSY);
        switch(old_val) {
            case PTHREAD_ONCE_INIT:
                // We're now in BUSY, so call the initialization function
                init_routine();
                if (!__sync_bool_compare_and_swap(once_control, PTHREAD_ONCE_BUSY, PTHREAD_ONCE_DONE))
                    abort();
                return 0;
            case PTHREAD_ONCE_BUSY:
                // Another thread is doing this; try again XXX We ought to sleep here
                continue;
            case PTHREAD_ONCE_DONE:
                return 0; // already done
            default:
                abort(); // corrupt!
        }
    }
    // NOTREACHED
}
