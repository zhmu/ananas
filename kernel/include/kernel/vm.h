/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types.h>

namespace vm::flag
{
    inline constexpr auto Read = (1 << 0); // read allowed
    inline constexpr auto Write = (1 << 1); // write allowed
    inline constexpr auto Execute = (1 << 2); // execute allowed
    inline constexpr auto Kernel = (1 << 3); // kernel-mode mapping
    inline constexpr auto User = (1 << 4); // user-mode mapping
    inline constexpr auto Device = (1 << 5); // device mapping
    inline constexpr auto Private = (1 << 6); // mapping not shared (used for inodes ??)
    inline constexpr auto MD = (1 << 31); // machine-dependent mapping
    inline constexpr auto Force = (1 << 30); // force mapping to be made
}
