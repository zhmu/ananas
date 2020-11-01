/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2020 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef ANANAS_X86_FB_H
#define ANANAS_X86_FB_H

#include <ananas/types.h>

struct ananas_fb_info {
    size_t fb_size;
    int fb_height;
    int fb_width;
    int fb_bpp;
};

// XXX We need some structure for this
#define TIOFB_GET_INFO 20000

#endif // ANANAS_X86_FB_H
