/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>

#ifndef __SLICE_H__
#define __SLICE_H__

class Device;

Device* slice_create(Device* parent, blocknr_t begin, blocknr_t length);

#endif /* __SLICE_H__ */
