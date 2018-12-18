#ifndef ANANAS_VCONSOLE_VGA_H
#define ANANAS_VCONSOLE_VGA_H

#include "ivideo.h"

class VGA : public IVideo
{
  public:
    VGA();
    virtual ~VGA() = default;

    int GetHeight() override;
    int GetWidth() override;

    void SetCursor(int x, int y) override;
    void PutPixel(int x, int y, const Pixel& pixel) override;

  private:
    void WriteCRTC(uint8_t reg, uint8_t val);

    uint32_t vga_io;
    volatile uint16_t* vga_video_mem;
};

#endif /* ANANAS_VCONSOLE_VGA_H */
