/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef ANANAS_VCONSOLE_FRAMEBUFFER_H
#define ANANAS_VCONSOLE_FRAMEBUFFER_H

#include "ivideo.h"

class Framebuffer : public IVideo
{
  public:
    Framebuffer();
    virtual ~Framebuffer() = default;

    int GetHeight() override;
    int GetWidth() override;

    void SetCursor(VTTY&, int x, int y) override;
    void PutPixel(VTTY&, int x, int y, const Pixel& pixel) override;

    Result IOControl(Process* proc, unsigned long req, void* buffer[]) override;
    Result
    DetermineDevicePhysicalAddres(addr_t& physAddress, size_t& length, int& mapFlags) override;

    static bool IsUsable();

  private:
    int fb_height;
    int fb_width;

    void DrawChar(int x, int y, uint32_t bgColor, uint32_t fgColor, int charcode);
    int fb_cursor_x = -1;
    int fb_cursor_y = -1;

    volatile uint32_t* fb_video_mem;
};

#endif // ANANAS_VCONSOLE_FRAMEBUFFER_H
