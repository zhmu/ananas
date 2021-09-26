#include "pixelbuffer.h"

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
    const auto activeRectangle = ClipTo(r, ::Rectangle{{}, size});

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
    const auto to = r.point + r.size;
    Line(Point{from.x, from.y}, Point{to.x, from.y}, colour);
    Line(Point{from.x, from.y}, Point{from.x, to.y}, colour);
    Line(Point{from.x, to.y}, Point{to.x, to.y}, colour);
    Line(Point{to.x, from.y}, Point{to.x, to.y}, colour);
}
