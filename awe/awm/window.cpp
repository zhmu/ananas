#include "window.h"
#include "awe/font.h"
#include "awe/pixelbuffer.h"
#include "palette.h"

#include <sys/shm.h>
#include <cstring>

namespace
{
    static awe::Handle nextHandle{1};

    auto GetNextWindowHandle()
    {
        auto result = nextHandle;
        ++nextHandle;
        return result;
    }
} // namespace

Window::Window(const awe::Point& point, const awe::Size& size, int fd)
    : handle(GetNextWindowHandle()), position(point), clientSize(size), fd(fd)
{
}

Window::~Window()
{
    if (shmData != nullptr)
        shmdt(shmData);
    if (shmId >= 0) {
        shmctl(shmId, IPC_RMID, NULL);
    }
}

void Window::Draw(awe::PixelBuffer& pb, awe::Font& font, const Palette& palette)
{
    using namespace awe;
    const auto& headerColour = focus ? palette.focusHeaderColour : palette.noFocusHeaderColour;

    const Rectangle frame{Point{position} - Point{borderWidth, headerHeight},
                          clientSize + Size{borderWidth, headerHeight}};
    pb.Rectangle(frame, palette.borderColour);

    const Rectangle header{Point{position} - Point{0, headerHeight} +
                               Point{borderWidth, borderWidth},
                           Size{clientSize.width - borderWidth, headerHeight - borderWidth}};
    pb.FilledRectangle(header, headerColour);

    const Rectangle client{Point{position}, clientSize};
    pb.FilledRectangle(client, palette.clientColour);

    font::DrawText(pb, font, header.point, Colour{255, 255}, title);

    if (shmData) {
        auto buffer = pb.GetBuffer();
        for (int y = 0; y < clientSize.height; ++y) {
            const auto offset = (y + position.y) * pb.GetSize().width + position.x;
            memcpy(
                &buffer[offset], &shmData[y * clientSize.width],
                clientSize.width * sizeof(PixelValue));
        }
    }
}

bool Window::HitsRectangle(const awe::Point& pos) const
{
    using namespace awe;
    const Rectangle windowRect{Point{position} - Point{0, headerHeight}, Size{clientSize} + Size{0, headerHeight}};
    return In(windowRect, pos);
}

bool Window::HitsHeaderRectangle(const awe::Point& pos) const
{
    using namespace awe;
    const Rectangle header{Point{position} - Point{0, headerHeight} +
                               Point{borderWidth, borderWidth},
                           Size{clientSize.width - borderWidth, headerHeight - borderWidth}};
    return In(header, pos);
}
