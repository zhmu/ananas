#pragma once

#include <memory>
#include "types.h"

namespace awe
{
    class PixelBuffer {
        const Size size;
        std::unique_ptr<PixelValue[]> storage;
        PixelValue* buffer;

    public:
        explicit PixelBuffer(Size size);
        PixelBuffer(PixelValue* ptr, Size size);

        void FilledRectangle(const struct Rectangle& r, const Colour& colour);
        const Size& GetSize() const { return size; }
        const PixelValue* GetBuffer() const { return buffer; }
        PixelValue* GetBuffer() { return buffer; }

        Colour GetPixel(const Point& point) const
        {
            if (!In(size, point))
                return {};
            return FromPixelValue(buffer[point.y * size.width + point.x]);
        }

        void PutPixel(const Point& point, const Colour& c)
        {
            if (In(size, point))
                buffer[point.y * size.width + point.x] = c;
        }

        void Line(const Point& from, const Point& to, const Colour& colour);
        void Rectangle(const Rectangle& r, const Colour& colour);

        void Blit(const awe::PixelBuffer& source, const awe::Rectangle& sourceRect, const awe::Point& dest);
        void Blend(const awe::PixelBuffer& src, const awe::Rectangle& srcRect, const awe::Point& dest);
    };
}
