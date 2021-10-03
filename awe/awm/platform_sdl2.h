#pragma once

#include <memory>
#include "platform.h"

class Platform_SDL2 : public Platform
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    Platform_SDL2();
    virtual ~Platform_SDL2();

    void Render(PixelBuffer& fb) override;
    std::optional<Event> Poll() override;
    Size GetSize() override;
};
