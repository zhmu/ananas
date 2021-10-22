#pragma once

#include <string>
#include "awe/types.h"

struct Palette;
struct PixelBuffer;

namespace awe
{
    class PixelBuffer;
    class Font;
}

class Window
{
  public:
    const awe::Handle handle;
    awe::Point position;
    awe::Size clientSize;
    std::string title;
    const int fd;
    int shmId{-1};
    uint32_t* shmData{};
    bool focus{};

    Window(const awe::Point&, const awe::Size&, int fd);
    ~Window();

    static inline constexpr int headerHeight = 20;
    static inline constexpr int borderWidth = 1;

    awe::Rectangle GetClientRectangle() const { return { position, clientSize }; }
    awe::Rectangle GetFrameRectangle() const;

    void Draw(awe::PixelBuffer& pb, awe::Font& font, const Palette& palette);
    bool HitsRectangle(const awe::Point& pos) const;
    bool HitsHeaderRectangle(const awe::Point& pos) const;
};
