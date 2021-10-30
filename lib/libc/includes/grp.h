/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct group {
    char* gr_name;
    gid_t gr_gid;
    char** gr_mem;
};

struct group* getgrgid(gid_t);
struct group* getgrnam(const char*);

struct group* getgrent();
void endgrent();
void setgrent();

#ifdef __cplusplus
}
#endif
