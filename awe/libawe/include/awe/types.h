#pragma once

#include <cstdint>
#include <algorithm>

namespace awe
{
    using PixelValue = uint32_t;
    using Handle = uint32_t;

    struct Colour {
        int r{255}, g{255}, b{255}, a{255};
        static constexpr Colour FromPixelValue(const PixelValue v)
        {
            return { static_cast<int>(v & 0xff),
                     static_cast<int>((v >> 8) & 0xff),
                     static_cast<int>((v >> 16) & 0xff),
                     static_cast<int>((v >> 24) & 0xff) };
        }

        constexpr operator PixelValue() const
        {
            return static_cast<PixelValue>(a) << 24 | static_cast<PixelValue>(b) << 16 |
                   static_cast<PixelValue>(g) << 8 | static_cast<PixelValue>(r);
        }
    };

    struct Point {
        int x{}, y{};
        bool operator==(const Point&) const = default;
    };

    struct Size {
        int width{}, height{};
        bool operator==(const Size&) const = default;
        bool empty() const { return width <= 0 || height <= 0; }
    };

    struct Rectangle {
        Point point;
        Size size;

        bool operator==(const Rectangle&) const = default;
        bool empty() const { return size.empty(); }
    };

    constexpr bool In(const Rectangle& rect, const Point& p)
    {
        return p.x >= rect.point.x && p.x < rect.point.x + rect.size.width && p.y >= rect.point.y &&
               p.y < rect.point.y + rect.size.height;
    }

    constexpr Colour FromPixelValue(const PixelValue v)
    {
        Colour result;
        result.r = static_cast<int>((v >> 0) & 0xff);
        result.g = static_cast<int>((v >> 8) & 0xff);
        result.b = static_cast<int>((v >> 16) & 0xff);
        result.a = static_cast<int>((v >> 24) & 0xff);
        return result;
    }

    constexpr Colour Blend(const Colour& source1, const Colour& source2, const float alpha)
    {
        Colour result;
        result.r = static_cast<int>(source1.r * (1.0 - alpha) + source2.r * alpha);
        result.g = static_cast<int>(source1.g * (1.0 - alpha) + source2.g * alpha);
        result.b = static_cast<int>(source1.b * (1.0 - alpha) + source2.b * alpha);
        return result;
    }

    constexpr awe::PixelValue BlendPixels(const awe::PixelValue src, const awe::PixelValue v)
    {
        const auto a = awe::Colour::FromPixelValue(src);
        const auto b = awe::Colour::FromPixelValue(v);
        const auto alpha = static_cast<float>(b.a) / 255.0f;
        return Blend(a, b, alpha);
    }

    constexpr bool In(const Size& size, const Point& p) { return In(Rectangle{{}, size}, p); }

    constexpr Point operator-(const Point& p) { return Point{-p.x, -p.y}; }

    constexpr Point operator+(const Point& p1, const Point& p2)
    {
        return Point{p1.x + p2.x, p1.y + p2.y};
    }

    constexpr Point operator-(const Point& p1, const Point& p2) { return p1 + (-p2); }

    constexpr Point operator+(const Point& p, const Size& s)
    {
        return Point{p.x + s.width, p.y + s.height};
    }

    constexpr Size operator-(const Size& s) { return Size{-s.width, -s.height}; }

    constexpr Size operator+(const Size& s1, const Size& s2)
    {
        return Size{s1.width + s2.width, s1.height + s2.height};
    }

    constexpr Size operator-(const Size& s1, const Size& s2) { return s1 + (-s2); }

    Rectangle Combine(const Rectangle& a, const Rectangle& b);
    Rectangle Intersect(const Rectangle& a, const Rectangle& b);
    Rectangle ClipTo(const Rectangle& r, const Rectangle& range);
}
