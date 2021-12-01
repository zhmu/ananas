/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <sys/types.h>
#include <ananas/tms.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

clock_t times(struct tms* buffer);

__END_DECLS
