#pragma once

#include <vector>
#include <memory>

#include "font.h"
#include "palette.h"
#include "pixelbuffer.h"
#include "platform.h"

class Window;

struct WindowManager final {
    struct EventProcessor;

    std::vector<std::unique_ptr<Window>> windows;
    bool needUpdate{true};
    PixelBuffer pixelBuffer;
    std::unique_ptr<Platform> platform;
    std::unique_ptr<EventProcessor> eventProcessor;
    Palette palette;
    font::Font font;

    WindowManager(std::unique_ptr<Platform> platform);
    ~WindowManager();

    void Update();
    Window* FindWindowAt(const Point& p);
    Window& CreateWindow(const Size& size, int fd);
    Window* FindWindowByHandle(const Handle handle);
    Window* FindWindowByFd(int fd);

    void DestroyWindow(Window&);

    bool Run();
};
