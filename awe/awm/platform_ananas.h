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

    void Render(PixelBuffer& fb) override;
    std::optional<Event> Poll() override;
    Size GetSize() override;

    int GetEventFd() const override;
};
