/*- * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/bootinfo.h>
#include "kernel/device.h"
#include "kernel/lib.h"
#include "kernel/kmem.h"
#include "kernel/vm.h"
#include "framebuffer.h"
#include "vtty.h"

#include "font.inc"

namespace font = unscii16;

namespace {
    constexpr uint32_t bgColour = 0x00000000;
    constexpr uint32_t fgColour = 0x0000ff00;
}

Framebuffer::Framebuffer()
{
    const auto fbSize = bootinfo->bi_video_xres * bootinfo->bi_video_yres * (bootinfo->bi_video_bpp / 8);
    void* fbMem = kmem_map(bootinfo->bi_video_framebuffer, fbSize, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_DEVICE);
    KASSERT(fbMem != nullptr, "cannot map framebuffer?");

    fb_height = bootinfo->bi_video_yres;
    fb_width = bootinfo->bi_video_xres;
    fb_video_mem = static_cast<uint32_t*>(fbMem);
}

bool Framebuffer::IsUsable()
{
    return bootinfo->bi_video_framebuffer != 0 && bootinfo->bi_video_bpp == 32; // only support 32-bpp for now
}

int Framebuffer::GetHeight() { return fb_height / font::charHeight; }

int Framebuffer::GetWidth() { return fb_width / font::charWidth; }

void Framebuffer::DrawChar(const int x, const int y, const uint32_t bgColour, const uint32_t fgColour, const int charCode)
{
    static_assert(font::charWidth <= 8, "TODO support wider chars");

    const auto charData = &font::fontData[charCode * font::bytesPerChar];
    for(int cy = 0; cy < font::charHeight; ++cy) {
        auto ptr = &fb_video_mem[fb_width * (cy + (y * font::charHeight)) + x * font::charWidth];
        for(int cx = 0; cx < font::charWidth; ++cx) {
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
    if (x == fb_cursor_x && y == fb_cursor_y) return;

    if (fb_cursor_x >= 0 && fb_cursor_y >= 0) {
        PutPixel(vtty, fb_cursor_x, fb_cursor_y, vtty.PixelAt(fb_cursor_x, fb_cursor_y));
    }

    fb_cursor_x = x;
    fb_cursor_y = y;
    if (fb_cursor_x >= 0 && fb_cursor_y >= 0) {
        DrawChar(fb_cursor_x, fb_cursor_y, fgColour, fgColour, 0);
    }
}
