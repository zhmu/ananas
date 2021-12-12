/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef ANANAS_LIMITS_H
#define ANANAS_LIMITS_H

/* Maximum number of descriptors per process */
#define PROCESS_MAX_DESCRIPTORS 64

/* Maximum length of a path */
#define PATH_MAX 256

// Non-standard, but appears to be used (i.e. llvm Path.inc)
#define MAXPATHLEN PATH_MAX

/* Host name */
#define HOST_NAME_MAX 64

#define _POSIX_ARG_MAX 4096

#define PTHREAD_KEYS_MAX 128

#endif /* ANANAS_LIMITS_H */
