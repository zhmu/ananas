#pragma once

#include <string>
#include "types.h"

struct Palette;
struct PixelBuffer;

namespace font
{
    class Font;
}

class Window
{
  public:
    const Handle handle;
    Point position;
    Size clientSize;
    std::string title;
    const int fd;
    int shmId{-1};
    uint32_t* shmData{};
    bool focus{};

    Window(const Point&, const Size&, int fd);
    ~Window();

    static inline constexpr int headerHeight = 20;
    static inline constexpr int borderWidth = 1;

    void Draw(PixelBuffer& pb, font::Font& font, const Palette& palette);
    bool HitsRectangle(const Point& pos) const;
    bool HitsHeaderRectangle(const Point& pos) const;
};
