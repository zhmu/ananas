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
    int shmId{-1};
    uint32_t* shmData{};

    Window(const Point&, const Size&);
    ~Window();

    static inline constexpr int headerHeight = 20;
    static inline constexpr int borderWidth = 1;

    void Draw(PixelBuffer& pb, font::Font& font, const Palette& palette);
    bool HitsHeaderRectangle(const Point& pos) const;
};