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

struct passwd {
    char* pw_name;
    uid_t pw_uid;
    gid_t pw_gid;
    char* pw_dir;
    char* pw_shell;
};

struct passwd* getpwnam(const char*);
struct passwd* getpwuid(uid_t);

void endpwent();
struct passwd* getpwent();
void setpwent();

#ifdef __cplusplus
}
#endif
