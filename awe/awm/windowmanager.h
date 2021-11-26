#pragma once

#include <vector>
#include <memory>

#include "awe/font.h"
#include "awe/pixelbuffer.h"
#include "palette.h"
#include "platform.h"

class Window;

struct WindowManager final {
    struct EventProcessor;

    std::vector<std::unique_ptr<Window>> windows;
    awe::Rectangle needUpdate;
    awe::PixelBuffer pixelBuffer;
    std::unique_ptr<Platform> platform;
    std::unique_ptr<EventProcessor> eventProcessor;
    Palette palette;
    awe::Font font;

    WindowManager(std::unique_ptr<Platform> platform);
    ~WindowManager();

    void Update();
    Window* FindWindowAt(const awe::Point& p);
    Window& CreateWindow(const awe::Point&, const awe::Size&, int fd);
    Window* FindWindowByHandle(const awe::Handle handle);
    Window* FindWindowByFd(int fd);

    awe::Point DeterminePositionFor(const awe::Size&) const;

    void Invalidate(const awe::Rectangle&);
    void DestroyWindow(Window&);
    void CycleFocus();

    bool Run();
    void HandlePlatformEvents();
};
