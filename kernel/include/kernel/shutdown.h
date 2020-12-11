/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

namespace shutdown
{
    enum class ShutdownType { Unknown, Halt, Reboot, PowerDown };

    bool IsShuttingDown();
    void RequestShutdown(ShutdownType type);

} // namespace shutdown
