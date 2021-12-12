/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

// XXX This is all just a temporary kludge until a more sane
// format is needed

#define IOCTL_SOUND_START 1
#define IOCTL_SOUND_STOP 2

#define SOUND_RATE_48KHZ 1 // 48.0kHz
#define SOUND_FORMAT_16S 1 // 16-bit signed

struct SOUND_START_ARGS {
    int ss_rate;   // any of SOUND_RATE_...
    int ss_format; // any of SOUND_FORMAT_...
    int ss_channels;
    int ss_buffer_length; // in pages
};
