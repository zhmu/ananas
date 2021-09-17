#pragma once

#include <optional>
#include <variant>
#include "types.h"

struct PixelBuffer;

namespace event
{
    enum class Button { Left, Right };

    struct Quit {
    };
    struct MouseButtonDown {
        Button button;
        Point position;
    };
    struct MouseButtonUp {
        Button button;
        Point position;
    };
    struct MouseMotion {
        Point position;
    };
} // namespace event

using Event =
    std::variant<event::Quit, event::MouseButtonDown, event::MouseButtonUp, event::MouseMotion>;

struct Platform {
    virtual ~Platform() = default;

    virtual Size GetSize() = 0;
    virtual void Render(PixelBuffer&) = 0;
    virtual std::optional<Event> Poll() = 0;
};
