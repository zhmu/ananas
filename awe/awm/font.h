#pragma once

#include <memory>
#include <string_view>
#include <vector>
#include "stb_truetype.h"

struct PixelBuffer;
struct Point;
struct Colour;

namespace font
{
    struct Glyph {
        int height{}, width{}, y0{}, advance{};
        std::vector<uint8_t> bitmap;
    };

    class Font
    {
        std::vector<Glyph> glyphs;
        stbtt_fontinfo fontInfo;
        float scale;
        int baseline;
        std::unique_ptr<unsigned char[]> data;

      public:
        Font(const char* path, const int fontSize);

        auto GetBaseLine() const { return baseline; }
        auto GetScale() const { return scale; }
        const Glyph& GetGlyph(const int ch);
    };

    void DrawText(
        PixelBuffer& pb, Font& font, const Point& p, const Colour& colour, std::string_view text);

} // namespace font
