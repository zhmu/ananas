#pragma once

#include <memory>
#include "platform.h"

class Platform_Ananas : public Platform
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    Platform_Ananas();
    virtual ~Platform_Ananas();

    void Render(awe::PixelBuffer&, const awe::Rectangle&) override;
    std::optional<Event> Poll() override;
    awe::Size GetSize() override;

    int GetEventFd() const override;
};
