/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "driver.h"

/*
 * Console drivers are 'special' as they will be probed during early boot and
 * skipped afterwards.
 *
 * The priority influences the order in which these things are attached, the
 * first successful driver wins and becomes the console driver. The order is
 * highest first, so use zero for the 'if all else fails'-driver.
 */
class ConsoleDriver : public Driver
{
  public:
    ConsoleDriver(const char* name, int priority) : Driver(name, priority) {}
    virtual ~ConsoleDriver() = default;

    ConsoleDriver* GetConsoleDriver() override { return this; }

    virtual Device* ProbeDevice() = 0;
};
