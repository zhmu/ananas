#include "awe/pixelbuffer.h"
#include <cstring>

namespace awe
{
    PixelBuffer::PixelBuffer(Size size) : size(std::move(size))
    {
        storage = std::make_unique<PixelValue[]>(size.height * size.width);
        buffer = storage.get();
    }

    PixelBuffer::PixelBuffer(PixelValue* p, Size size)
        : buffer(p), size(std::move(size))
    {
    }

    void PixelBuffer::FilledRectangle(const struct Rectangle& r, const Colour& colour)
    {
        const auto activeRectangle = ClipTo(r, awe::Rectangle{{}, size});

        auto ptr = &buffer[activeRectangle.point.y * size.width + activeRectangle.point.x];
        for (auto y = 0; y < activeRectangle.size.height; ++y) {
            for (auto x = 0; x < activeRectangle.size.width; ++x) {
                *ptr++ = colour;
            }
            ptr += size.width - activeRectangle.size.width;
        }
    }

    void PixelBuffer::Line(const Point& from, const Point& to, const Colour& colour)
    {
        // Bresenham algorithm from https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
        const auto abs = [](int x) { return x >= 0 ? x : -x; };

        const auto dx = abs(to.x - from.x);
        const auto dy = -abs(to.y - from.y);
        const auto sx = from.x < to.x ? 1 : -1;
        const auto sy = from.y < to.y ? 1 : -1;
        auto err = dx + dy;

        Point current{from};
        while (true) {
            PutPixel(current, colour);
            if (current == to)
                break;
            const auto e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                current.x += sx;
            }
            if (e2 <= dx) {
                err += dx;
                current.y += sy;
            }
        }
    }

    void PixelBuffer::Rectangle(const struct Rectangle& r, const Colour& colour)
    {
        const auto from = r.point;
        const auto to = r.point + r.size - Point{ 1, 1 };

        Line(Point{from.x, from.y}, Point{to.x, from.y}, colour);
        Line(Point{from.x, from.y}, Point{from.x, to.y}, colour);
        Line(Point{from.x, to.y}, Point{to.x, to.y}, colour);
        Line(Point{to.x, from.y}, Point{to.x, to.y}, colour);
    }

    namespace
    {
        auto DetermineRectAndPoint(const awe::Size& srcSize, const awe::Size& dstSize, const awe::Rectangle& srcRect, const awe::Point& destPoint)
        {
            auto rect = srcRect;
            auto point = destPoint;
            if (const auto deltaX = -point.x; deltaX > 0) {
                rect.point.x += deltaX;
                rect.size.width -= deltaX;
                point.x = 0;
            }
            if (const auto deltaY = -point.y; deltaY > 0) {
                rect.point.y += deltaY;
                rect.size.height -= deltaY;
                point.y = 0;
            }
            rect = ClipTo(rect, { {}, srcSize });
            if (point.x + rect.size.width > dstSize.width) {
                rect.size.width = dstSize.width - point.x;
            }
            if (point.y + rect.size.height > dstSize.height) {
                rect.size.height = dstSize.height - point.y;
            }
            return std::pair{ rect, point };
        }

        template<typename PB>
        auto DeterminePixelPointer(PB& pb, const awe::Point& point)
        {
            return &pb.GetBuffer()[pb.GetSize().width * point.y + point.x];
        }
    }

    void PixelBuffer::Blit(const awe::PixelBuffer& src, const awe::Rectangle& srcRect, const awe::Point& dest)
    {
        const auto [ rect, point ] = DetermineRectAndPoint(src.size, size, srcRect, dest);
        auto srcPointer = DeterminePixelPointer(src, rect.point);
        auto dstPointer = DeterminePixelPointer(*this, point);

        for (int y = 0; y < rect.size.height; ++y) {
            std::memcpy(dstPointer, srcPointer, rect.size.width * sizeof(awe::PixelValue));
            srcPointer += src.size.width;
            dstPointer += size.width;
        }
    }

    void PixelBuffer::Blend(const awe::PixelBuffer& src, const awe::Rectangle& srcRect, const awe::Point& dest)
    {
        const auto [ rect, point ] = DetermineRectAndPoint(src.size, size, srcRect, dest);
        auto srcPointer = DeterminePixelPointer(src, rect.point);
        auto dstPointer = DeterminePixelPointer(*this, point);

        for (int y = 0; y < rect.size.height; ++y) {
            for (int x = 0; x < rect.size.width; ++x) {
                *dstPointer = awe::BlendPixels(*dstPointer, *srcPointer);
                ++dstPointer, ++srcPointer;
            }
            srcPointer += (src.size.width - rect.size.width);
            dstPointer += (size.width - rect.size.width);
        }
    }
}
