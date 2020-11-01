/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef ANANAS_VCONSOLE_IVIDEO_H
#define ANANAS_VCONSOLE_IVIDEO_H

#include "../../../lib/teken/teken.h"

class VTTY;

class IVideo
{
  public:
    struct Pixel {
        Pixel() = default;
        Pixel(teken_char_t c, teken_attr_t a) : p_c(c), p_a(a) {}

        teken_char_t p_c{0};
        teken_attr_t p_a{0, 0, 0};
    };

    virtual ~IVideo() = default;

    virtual int GetHeight() = 0;
    virtual int GetWidth() = 0;

    virtual void SetCursor(VTTY&, int x, int y) = 0;
    virtual void PutPixel(VTTY&, int x, int y, const Pixel& pixel) = 0;

    virtual Result IOControl(Process* proc, unsigned long req, void* buffer[]) = 0;
    virtual Result
    DetermineDevicePhysicalAddres(addr_t& physAddress, size_t& length, int& mapFlags) = 0;
};

#endif /* ANANAS_VCONSOLE_IVIDEO_H */
