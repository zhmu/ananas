/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __ANANAS_USB_CONFIG_H__
#define __ANANAS_USB_CONFIG_H__

class Result;

namespace usb
{
    class Interface;

    Result ParseConfiguration(Interface* usb_if, int& usb_num_if, void* data, int datalen);

} // namespace usb

#endif
