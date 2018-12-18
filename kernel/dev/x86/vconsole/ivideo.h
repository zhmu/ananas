#ifndef ANANAS_VCONSOLE_IVIDEO_H
#define ANANAS_VCONSOLE_IVIDEO_H

#include "../../../lib/teken/teken.h"

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

    virtual void SetCursor(int x, int y) = 0;
    virtual void PutPixel(int x, int y, const Pixel& pixel) = 0;
};

#endif /* ANANAS_VCONSOLE_IVIDEO_H */
