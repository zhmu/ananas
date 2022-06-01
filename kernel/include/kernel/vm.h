/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

namespace vm::flag
{
    inline constexpr auto Read = (1 << 0); // read allowed
    inline constexpr auto Write = (1 << 1); // write allowed
    inline constexpr auto Execute = (1 << 2); // execute allowed
    inline constexpr auto Kernel = (1 << 3); // kernel-mode mapping
    inline constexpr auto User = (1 << 4); // user-mode mapping
    inline constexpr auto Device = (1 << 5); // device mapping
    inline constexpr auto Private = (1 << 6); // memory will not be shared with parent when fork()-ing
    inline constexpr auto Force = (1 << 30); // force mapping to be made
}
