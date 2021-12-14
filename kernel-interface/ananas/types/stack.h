/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

struct sigaltstack {
    void* ss_sp;
    int ss_flags;
    size_t ss_size;
};

typedef struct sigaltstack stack_t;
