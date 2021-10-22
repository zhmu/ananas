#include "awe/types.h"

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <cstring>
#include <cstdarg>

namespace awe
{
    namespace
    {
        auto RectangleToCoordinates(const Rectangle& r)
        {
            const auto x1 = r.point.x;
            const auto y1 = r.point.y;
            const auto x2 = x1 + r.size.width;
            const auto y2 = y1 + r.size.height;
            return std::tuple{ x1, y1, x2, y2 };
        }

        template<typename T> T Clamp(const T value, T minValue, T maxValue)
        {
            return std::min(std::max(value, minValue), maxValue);
        }
    }

    Rectangle Combine(const Rectangle& a, const Rectangle& b)
    {
        if (a.empty()) return b;
        if (b.empty()) return a;

        const auto [ xa1, ya1, xa2, ya2 ] = RectangleToCoordinates(a);
        const auto [ xb1, yb1, xb2, yb2 ] = RectangleToCoordinates(b);

        const auto x1 = std::min(xa1, xb1);
        const auto y1 = std::min(ya1, yb1);
        const auto x2 = std::max(xa2, xb2);
        const auto y2 = std::max(ya2, yb2);
        return { { x1, y1 }, { { x2 - x1 }, { y2 - y1 } } };
    }

    Rectangle Intersect(const Rectangle& a, const Rectangle& b)
    {
        if (a.empty()) return {};
        if (b.empty()) return {};

        const auto [ xa1, ya1, xa2, ya2 ] = RectangleToCoordinates(a);
        const auto [ xb1, yb1, xb2, yb2 ] = RectangleToCoordinates(b);

        const auto x1 = std::max(xa1, xb1);
        const auto y1 = std::max(ya1, yb1);
        const auto x2 = std::min(xa2, xb2);
        const auto y2 = std::min(ya2, yb2);
        return { { x1, y1 }, { { x2 - x1 }, { y2 - y1 } } };
    }

    Rectangle ClipTo(const Rectangle& r, const Rectangle& range)
    {
        const auto [ rx1, ry1, rx2, ry2 ] = RectangleToCoordinates(r);
        const auto [ ax1, ay1, ax2, ay2 ] = RectangleToCoordinates(range);

        const auto x1 = Clamp(rx1, ax1, ax2);
        const auto x2 = Clamp(rx2, ax1, ax2);
        const auto y1 = Clamp(ry1, ay1, ay2);
        const auto y2 = Clamp(ry2, ay1, ay2);
        return { { x1, y1 }, { { x2 - x1 }, { y2 - y1 } } };
    }
}
