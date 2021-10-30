/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/x86/fb.h>
#include "kernel/device.h"
#include "kernel/lib.h"
#include "kernel/pcpu.h"
#include "kernel/process.h"
#include "kernel/kmem.h"
#include "kernel/thread.h"
#include "kernel/vm.h"
#include "kernel/vmspace.h"
#include "framebuffer.h"
#include "vtty.h"

#include "font.inc"

extern int video_resolution_width;
extern int video_resolution_height;
extern addr_t video_framebuffer;

namespace font = unscii16;

namespace
{
    constexpr uint32_t bgColour = 0x00000000;
    constexpr uint32_t fgColour = 0x0000ff00;
    constexpr uint32_t supportedBpp = 32;
} // namespace

Framebuffer::Framebuffer()
{
    const auto fbSize = video_resolution_width * video_resolution_height * sizeof(uint32_t);
    void* fbMem = kmem_map(
        video_framebuffer, fbSize, vm::flag::Read | vm::flag::Write | vm::flag::Device);
    KASSERT(fbMem != nullptr, "cannot map framebuffer?");

    fb_height = video_resolution_height;
    fb_width = video_resolution_width;
    fb_video_mem = static_cast<uint32_t*>(fbMem);
}

bool Framebuffer::IsUsable()
{
    return video_framebuffer != 0;
}

int Framebuffer::GetHeight() { return fb_height / font::charHeight; }

int Framebuffer::GetWidth() { return fb_width / font::charWidth; }

void Framebuffer::DrawChar(
    const int x, const int y, const uint32_t bgColour, const uint32_t fgColour, const int charCode)
{
    static_assert(font::charWidth <= 8, "TODO support wider chars");

    const auto charData = &font::fontData[charCode * font::bytesPerChar];
    for (int cy = 0; cy < font::charHeight; ++cy) {
        auto ptr = &fb_video_mem[fb_width * (cy + (y * font::charHeight)) + x * font::charWidth];
        for (int cx = 0; cx < font::charWidth; ++cx) {
            uint32_t colour = bgColour;
            if (charData[cy] & (1 << (8 - cx)))
                colour = fgColour;
            *ptr++ = colour;
        }
    }
}

void Framebuffer::PutPixel(VTTY& vtty, int x, int y, const Pixel& px)
{
    DrawChar(x, y, bgColour, fgColour, px.p_c & 255);
}

void Framebuffer::SetCursor(VTTY& vtty, int x, int y)
{
    if (x == fb_cursor_x && y == fb_cursor_y)
        return;

    if (fb_cursor_x >= 0 && fb_cursor_y >= 0) {
        PutPixel(vtty, fb_cursor_x, fb_cursor_y, vtty.PixelAt(fb_cursor_x, fb_cursor_y));
    }

    fb_cursor_x = x;
    fb_cursor_y = y;
    if (fb_cursor_x >= 0 && fb_cursor_y >= 0) {
        DrawChar(fb_cursor_x, fb_cursor_y, fgColour, fgColour, 0);
    }
}

Result Framebuffer::IOControl(Process* proc, unsigned long req, void* buffer[])
{
    switch (req) {
        case TIOFB_GET_INFO: {
            auto fb_info = static_cast<ananas_fb_info*>(buffer[0]); // XXX userland check
            if (fb_info == NULL || fb_info->fb_size != sizeof(*fb_info))
                return Result::Failure(EINVAL);

            fb_info->fb_height = fb_height;
            fb_info->fb_width = fb_width;
            fb_info->fb_bpp = supportedBpp;
            return Result::Success();
        }
    }

    return Result::Failure(EINVAL);
}

Result
Framebuffer::DetermineDevicePhysicalAddres(addr_t& physAddress, size_t& length, int& mapFlags)
{
    const auto fbSize = fb_height * fb_width * (supportedBpp / 8);
    if (length > fbSize)
        length = fbSize;

    physAddress = video_framebuffer;
    return Result::Success();
}
