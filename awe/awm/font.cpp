#include "font.h"
#include "types.h"

#include <fcntl.h>
#include <unistd.h>

#include "pixelbuffer.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace font
{
    Font::Font(const char* path, const int fontSize) : fontSize(fontSize)
    {
        int fd = open(path, O_RDONLY);
        if (fd < 0)
            throw std::runtime_error("cannot open font file");
        const auto len = [&]() {
            const auto len = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            return len;
        }();
        data = std::make_unique<unsigned char[]>(len);
        read(fd, data.get(), len);
        close(fd);

        stbtt_InitFont(&fontInfo, data.get(), stbtt_GetFontOffsetForIndex(data.get(), 0));
        scale = stbtt_ScaleForPixelHeight(&fontInfo, fontSize);
        int ascent;
        stbtt_GetFontVMetrics(&fontInfo, &ascent, 0, 0);
        baseline = static_cast<int>(ascent * scale);

        glyphs.resize(256);
    }

    const Glyph& Font::GetGlyph(const int ch)
    {
        auto& glyph = glyphs[ch];
        if (glyph.bitmap.empty()) {
            int h, w;
            const auto bitmap = stbtt_GetCodepointBitmap(&fontInfo, 0, scale, ch, &w, &h, 0, 0);
            float x_shift = 0.0f;
            int x0, y0, x1, y1;
            stbtt_GetCodepointBitmapBoxSubpixel(
                &fontInfo, ch, scale, scale, x_shift, 0, &x0, &y0, &x1, &y1);
            int advance, lsb;
            stbtt_GetCodepointHMetrics(&fontInfo, ch, &advance, &lsb);

            glyph = Glyph{h, w, y0, advance, std::vector<uint8_t>(&bitmap[0], &bitmap[h * w])};
        }
        return glyph;
    }

    void DrawText(
        PixelBuffer& pb, Font& font, const Point& p, const Colour& colour, std::string_view text)
    {
        Point current{p};
        const auto baseline = font.GetBaseLine();
        for (const auto ch : text) {
            const auto& glyph = font.GetGlyph(ch);

            const Point renderPoint{current + Point{0, baseline + glyph.y0}};
            for (int y = 0; y < glyph.height; ++y) {
                for (int x = 0; x < glyph.width; ++x) {
                    const auto v = glyph.bitmap[y * glyph.width + x] / 255.0;
                    const auto currentPixel = pb.GetPixel(renderPoint + Point{x, y});

                    const auto scaledColour = Blend(currentPixel, colour, v);
                    pb.PutPixel(renderPoint + Point{x, y}, scaledColour);
                }
            }
            current.x += glyph.advance * font.GetScale();
        }
    }

} // namespace font
