/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/util/list.h>

struct AIMX_EVENT;

namespace input_mux
{
    class IInputConsumer : public util::List<IInputConsumer>::NodePtr
    {
      public:
        virtual void OnEvent(const AIMX_EVENT&) = 0;
    };

    void RegisterConsumer(IInputConsumer& consumer);
    void UnregisterConsumer(IInputConsumer& consumer);
    void OnEvent(const AIMX_EVENT&);

} // namespace input_mux
