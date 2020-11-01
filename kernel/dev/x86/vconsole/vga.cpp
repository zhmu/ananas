/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel-md/io.h"
#include "kernel/device.h"
#include "kernel/lib.h"
#include "kernel/trace.h"
#include "vga.h"

TRACE_SETUP;

#define VGA_HEIGHT 25
#define VGA_WIDTH 80

VGA::VGA()
{
    // XXX This is a quite ugly
    ResourceSet resourceSet;
    resourceSet.AddResource(Resource(Resource::RT_Memory, 0xb8000, 128 * 1024));
    resourceSet.AddResource(Resource(Resource::RT_IO, 0x3c0, 32));
    void* mem = resourceSet.AllocateResource(Resource::RT_Memory, 0x7fff);
    void* res = resourceSet.AllocateResource(Resource::RT_IO, 0x1f);
    KASSERT(mem != nullptr && res != nullptr, "no resources?");

    vga_io = (uintptr_t)res;
    vga_video_mem = static_cast<uint16_t*>(mem);

    // Clear the display; we must set the attribute everywhere for the cursor
    {
        auto* ptr = vga_video_mem;
        int size = VGA_HEIGHT * VGA_WIDTH;
        while (size--) {
            *ptr++ = (0xf /* white */ << 8) | ' ';
        }
    }

    // Set cursor shape
    WriteCRTC(0xa, 14);
    WriteCRTC(0xb, 15);
}

int VGA::GetHeight() { return VGA_HEIGHT; }

int VGA::GetWidth() { return VGA_WIDTH; }

void VGA::WriteCRTC(uint8_t reg, uint8_t val)
{
    outb(vga_io + 0x14, reg);
    outb(vga_io + 0x15, val);
}

void VGA::PutPixel(VTTY&, int x, int y, const Pixel& px)
{
    *(volatile uint16_t*)(vga_video_mem + y * VGA_WIDTH + x) =
        (uint16_t)(teken_256to8(px.p_a.ta_fgcolor) + 8 * teken_256to8(px.p_a.ta_bgcolor)) << 8 |
        px.p_c;
}

void VGA::SetCursor(VTTY&, int x, int y)
{
    uint32_t offs = x + y * VGA_WIDTH;
    WriteCRTC(0xe, offs >> 8);
    WriteCRTC(0xf, offs & 0xff);
}

Result VGA::IOControl(Process* proc, unsigned long req, void* buffer[])
{
    return RESULT_MAKE_FAILURE(EINVAL);
}

Result VGA::DetermineDevicePhysicalAddres(addr_t& physAddress, size_t& length, int& mapFlags)
{
    return RESULT_MAKE_FAILURE(EINVAL);
}
