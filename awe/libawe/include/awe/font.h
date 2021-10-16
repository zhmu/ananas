#pragma once

#include <memory>
#include <string_view>
#include <vector>

namespace awe
{
    struct PixelBuffer;
    struct Point;
    struct Colour;

    struct Glyph {
        int height{}, width{}, y0{}, advance{};
        std::vector<uint8_t> bitmap;
    };

    class Font
    {
        struct Impl;
        std::unique_ptr<Impl> impl;

      public:
        Font(const char* path, const int fontSize);
        ~Font();

        int GetBaseLine() const;
        float GetScale() const;
        const Glyph& GetGlyph(const int ch);
    };

    namespace font
    {
        void DrawText(
            PixelBuffer& pb, Font& font, const Point& p, const Colour& colour, std::string_view text);
    }
}
