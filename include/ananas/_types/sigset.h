/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __SIGSET_T_DEFINED
/* Broad definition to allow future extension (i.e. real-time signals) once we need them */
typedef struct {
    unsigned int sig[1];
} sigset_t;
#define __SIGSET_T_DEFINED
#endif
