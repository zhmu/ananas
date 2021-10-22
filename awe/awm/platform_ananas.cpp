#include "platform_ananas.h"
#include <ananas/syscalls.h>
#include <ananas/x86/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <cstring>
#include <ananas/inputmux.h>
#include "awe/pixelbuffer.h"
#include "mouse.h"

#include <cstdarg>

namespace
{
    const awe::Colour mouseColour{ 255, 255, 0 };
    const awe::Size mouseSize{ mouse::cursorWidth, mouse::cursorHeight };

    std::pair<awe::ipc::KeyCode, int> ConvertKeycode(const AIMX_KEY_CODE key)
    {
        using namespace awe::ipc;
        const auto ch = (key >= 1 && key <= 127) ? key : 0;
        switch(key) {
            case AIMX_KEY_A ... AIMX_KEY_Z: return { static_cast<KeyCode>(static_cast<int>(KeyCode::A) - (key - AIMX_KEY_A)), ch };
            case AIMX_KEY_0 ... AIMX_KEY_9: return { static_cast<KeyCode>(static_cast<int>(KeyCode::n0) - (key - AIMX_KEY_0)), ch };
            case AIMX_KEY_F1 ... AIMX_KEY_F12: return { static_cast<KeyCode>(static_cast<int>(KeyCode::F1) - (key - AIMX_KEY_F1)), ch };
            case AIMX_KEY_ENTER: return { KeyCode::Enter, '\r' };
            case AIMX_KEY_ESCAPE: return { KeyCode::Escape, 27 };
            case AIMX_KEY_SPACE: return { KeyCode::Space, ' ' };
            case AIMX_KEY_TAB: return { KeyCode::Tab, '\t' };
            case AIMX_KEY_LEFT_ARROW: return { KeyCode::LeftArrow, 0 };
            case AIMX_KEY_RIGHT_ARROW: return { KeyCode::RightArrow, 0 };
            case AIMX_KEY_UP_ARROW: return { KeyCode::UpArrow, 0 };
            case AIMX_KEY_DOWN_ARROW: return { KeyCode::DownArrow, 0 };
            case AIMX_KEY_LEFT_CONTROL: return { KeyCode::LeftControl, 0 };
            case AIMX_KEY_LEFT_SHIFT: return { KeyCode::LeftShift, 0 };
            case AIMX_KEY_LEFT_ALT: return { KeyCode::LeftAlt, 0 };
            case AIMX_KEY_LEFT_GUI: return { KeyCode::LeftGUI, 0 };
            case AIMX_KEY_RIGHT_CONTROL: return { KeyCode::RightControl, 0 };
            case AIMX_KEY_RIGHT_SHIFT: return { KeyCode::RightShift, 0 };
            case AIMX_KEY_RIGHT_ALT: return { KeyCode::RightAlt, 0 };
            case AIMX_KEY_RIGHT_GUI: return { KeyCode::RightGUI, 0 };
        }
        return { KeyCode::Unknown, ch };
    }

    int ConvertModifiers(const AIMX_MODIFIERS mods)
    {
        using namespace awe::ipc;
        int result = 0;

        auto mapModifier = [&](int aimxBit, int ipcBit) {
            if (aimxBit & AIMX_MOD_LEFT_CONTROL)
                result |= ipcBit;
        };

        mapModifier(AIMX_MOD_LEFT_CONTROL, keyModifiers::LeftControl);
        mapModifier(AIMX_MOD_LEFT_SHIFT, keyModifiers::LeftShift);
        mapModifier(AIMX_MOD_LEFT_ALT, keyModifiers::LeftAlt);
        mapModifier(AIMX_MOD_LEFT_GUI, keyModifiers::LeftGUI);
        mapModifier(AIMX_MOD_RIGHT_CONTROL, keyModifiers::RightControl);
        mapModifier(AIMX_MOD_RIGHT_SHIFT, keyModifiers::RightShift);
        mapModifier(AIMX_MOD_RIGHT_ALT, keyModifiers::RightAlt);
        mapModifier(AIMX_MOD_RIGHT_GUI, keyModifiers::RightGUI);
        return result;
    }

    void MakeRaw(struct termios& ts)
    {
        ts.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
        ts.c_oflag &= ~OPOST;
        ts.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
        ts.c_cflag &= ~(CSIZE | PARENB);
        ts.c_cflag |= CS8;
    }
}

struct Platform_Ananas::Impl {
    int consoleFd{-1};
    int inputFd{-1};
    awe::Size size{};
    awe::Point mousePos;
    awe::Point renderedMousePos{-1, -1};
    bool mouseLeftButton{};
    bool mouseRightButton{};
    std::unique_ptr<awe::PixelBuffer> fb;

    Impl()
    {
        const auto* tty = ttyname(STDIN_FILENO);
        if (tty == nullptr) throw std::runtime_error("cannot obtain tty device");

        consoleFd = open(tty, O_RDWR);
        if (consoleFd < 0) perror("open console");

        inputFd = open("/dev/inputmux", O_RDONLY | O_NONBLOCK);
        if (inputFd < 0) perror("open inputmux");

        struct ananas_fb_info fbi{};
        fbi.fb_size = sizeof(fbi);
        if (ioctl(consoleFd, TIOFB_GET_INFO, &fbi) < 0) throw std::runtime_error("cannot obtain fb info");

        size.height = fbi.fb_height;
        size.width = fbi.fb_width;
        if (fbi.fb_bpp != sizeof(awe::PixelValue) * 8) throw std::runtime_error("unsupported fb bpp");

        auto fbPointer = static_cast<awe::PixelValue*>(mmap(NULL, size.width * size.height * sizeof(awe::PixelValue), PROT_READ | PROT_WRITE, MAP_SHARED, consoleFd, 0));
        if (fbPointer == static_cast<awe::PixelValue*>(MAP_FAILED)) throw std::runtime_error("cannot map fb");

        struct termios ts;
        if (tcgetattr(consoleFd, &ts) < 0) throw std::runtime_error("cannot get term attributes");
        MakeRaw(ts);
        if (tcsetattr(consoleFd, TCSANOW, &ts) < 0) throw std::runtime_error("cannot set term attributes");

        fb = std::make_unique<awe::PixelBuffer>(fbPointer, size);
    }

    ~Impl()
    {
        munmap(fb.get(), size.width * size.height * sizeof(awe::PixelValue));
        close(consoleFd);
    }

    void RenderMouseCursor(awe::PixelBuffer& pb, const awe::Point& p)
    {
        const awe::Size cursorSize{ mouse::cursorWidth, mouse::cursorHeight };

        awe::PixelBuffer iconBuffer{ const_cast<awe::PixelValue*>(&mouse::icon[0]), cursorSize };
        fb->Blend(iconBuffer, { {}, cursorSize }, p);
    }

    void Render(awe::PixelBuffer& pb, const awe::Rectangle& rect)
    {
        fb->Blit(pb, rect, rect.point);

        if (renderedMousePos.x >= 0) {
            fb->Blit(pb, { renderedMousePos, mouseSize }, renderedMousePos);
        }
        RenderMouseCursor(*fb, mousePos);
        renderedMousePos = mousePos;
    }

    std::optional<Event> Poll()
    {
        struct AIMX_EVENT event;
        const auto n = read(inputFd, &event, sizeof(event));
        if (n != sizeof(event)) return {};

        switch(event.type) {
            case AIMX_EVENT_KEY_DOWN: {
                const auto key = ConvertKeycode(event.u.key_down.key);
                const auto modifier = ConvertModifiers(event.u.key_down.mods);
                return event::KeyDown{key.first, modifier, key.second};
            }
            case AIMX_EVENT_KEY_UP: {
                const auto key = ConvertKeycode(event.u.key_up.key);
                const auto modifier = ConvertModifiers(event.u.key_up.mods);
                return event::KeyUp{key.first, modifier, key.second};
            }
            case AIMX_EVENT_MOUSE: {
                const auto& mouse = event.u.mouse;
                const auto currentLeftDown = (mouse.button & AIMX_BUTTON_LEFT) != 0;
                const auto currentRightDown = (mouse.button & AIMX_BUTTON_RIGHT) != 0;

                mousePos.x += mouse.delta_x;
                mousePos.y += mouse.delta_y;
                if (mousePos.x < 0) mousePos.x = 0;
                if (mousePos.x >= size.width) mousePos.x = size.width - 1;
                if (mousePos.y < 0) mousePos.y = 0;
                if (mousePos.y >= size.height) mousePos.y = size.height - 1;

                if (mouseLeftButton != currentLeftDown) {
                    if (currentLeftDown) {
                        mouseLeftButton = true;
                        return event::MouseButtonDown{event::Button::Left, mousePos};
                    }
                    mouseLeftButton = false;
                    return event::MouseButtonUp{event::Button::Left, mousePos};
                }
                if (mouseRightButton != currentRightDown) {
                    if (currentRightDown) {
                        mouseRightButton = true;
                        return event::MouseButtonDown{event::Button::Right, mousePos};
                    }
                    mouseRightButton = false;
                    return event::MouseButtonUp{event::Button::Right, mousePos};
                }
                return event::MouseMotion{ mousePos };
            }
        }
        return {};
    }
};

Platform_Ananas::Platform_Ananas() : impl(std::make_unique<Impl>()) {}

Platform_Ananas::~Platform_Ananas() = default;

void Platform_Ananas::Render(awe::PixelBuffer& fb, const awe::Rectangle& rect) { return impl->Render(fb, rect); }

std::optional<Event> Platform_Ananas::Poll() { return impl->Poll(); }

awe::Size Platform_Ananas::GetSize() { return impl->size; }

int Platform_Ananas::GetEventFd() const { return impl->inputFd; }
