#pragma once

#include <optional>
#include <variant>
#include "awe/types.h"
#include "awe/ipc.h"

namespace awe
{
    class PixelBuffer;
}

namespace event
{
    enum class Button { Left, Right };

    struct Quit {
    };
    struct MouseButtonDown {
        Button button;
        awe::Point position;
    };
    struct MouseButtonUp {
        Button button;
        awe::Point position;
    };
    struct MouseMotion {
        awe::Point position;
    };
    struct KeyDown {
        awe::ipc::KeyCode key;
        int mods;
        int ch;
    };
    struct KeyUp {
        awe::ipc::KeyCode key;
        int mods;
        int ch;
    };
} // namespace event

using Event =
    std::variant<event::Quit, event::MouseButtonDown, event::MouseButtonUp, event::MouseMotion, event::KeyDown, event::KeyUp>;

struct Platform {
    virtual ~Platform() = default;

    virtual awe::Size GetSize() = 0;
    virtual void Render(awe::PixelBuffer&, const awe::Rectangle&) = 0;
    virtual std::optional<Event> Poll() = 0;

    virtual int GetEventFd() const { return -1; }
};
