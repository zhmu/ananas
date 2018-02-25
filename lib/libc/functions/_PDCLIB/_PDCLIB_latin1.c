/* Latin-1 codec

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdbool.h>
#include <uchar.h>
#include "_PDCLIB_encoding.h"

static bool latin1_mbsinit( const mbstate_t *ps )
{ return 1; }

static bool latin1toc32(
    char32_t       *restrict *restrict   p_outbuf,
    size_t                   *restrict   p_outsz,
    const char     *restrict *restrict   p_inbuf,
    size_t                   *restrict   p_insz,
    mbstate_t                *restrict   p_ps
)
{
    while(*p_outsz && *p_insz) {
        unsigned char c = **p_inbuf;

        if(p_outbuf) {
            **p_outbuf = c;
            (*p_outbuf)++; 
        }

        (*p_inbuf)++;
        (*p_insz)--; 
        (*p_outsz)--;
    }
    return true;
}

static bool c32tolatin1(
    char           *restrict *restrict  p_outbuf,
    size_t                   *restrict  p_outsz,
    const char32_t *restrict *restrict  p_inbuf,
    size_t                   *restrict  p_insz,
    mbstate_t                *restrict  p_ps
)
{
    while(*p_outsz && *p_insz) {
        char32_t c = **p_inbuf;
        if(c > 255)
            return false;

        if(p_outbuf) {
            **p_outbuf = c;
            (*p_outbuf)++;
        }

        (*p_inbuf)++;
        (*p_insz)--;
        (*p_outsz)--;
    }
    return true;
}

const struct _PDCLIB_charcodec_t _PDCLIB_latin1_codec = {
    .__mbsinit   = latin1_mbsinit,
    .__mbstoc32s = latin1toc32,
    .__c32stombs = c32tolatin1,
    .__mb_max    = 1,
};
