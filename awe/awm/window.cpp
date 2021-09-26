#include "window.h"
#include "font.h"
#include "palette.h"
#include "pixelbuffer.h"

#include <sys/shm.h>
#include <cstring>

namespace
{
    static Handle nextHandle{1};

    auto GetNextWindowHandle()
    {
        auto result = nextHandle;
        ++nextHandle;
        return result;
    }
} // namespace

Window::Window(const Point& point, const Size& size, int fd)
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

void Window::Draw(PixelBuffer& pb, font::Font& font, const Palette& palette)
{
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
        for (int y = 0; y < clientSize.height; ++y) {
            const auto offset = (y + position.y) * pb.size.width + position.x;
            memcpy(
                &pb.buffer[offset], &shmData[y * clientSize.width],
                clientSize.width * sizeof(PixelValue));
        }
    }
}

bool Window::HitsRectangle(const Point& pos) const
{
    const Rectangle windowRect{Point{position} - Point{0, headerHeight}, Size{clientSize} + Size{0, headerHeight}};
    return In(windowRect, pos);
}

bool Window::HitsHeaderRectangle(const Point& pos) const
{
    const Rectangle header{Point{position} - Point{0, headerHeight} +
                               Point{borderWidth, borderWidth},
                           Size{clientSize.width - borderWidth, headerHeight - borderWidth}};
    return In(header, pos);
}
