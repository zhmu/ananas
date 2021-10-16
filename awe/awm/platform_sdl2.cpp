#include "platform_sdl2.h"
#include <SDL.h>
#include "awe/pixelbuffer.h"
#include "awe/ipc.h"

namespace
{
    inline constexpr Size size{1024, 768};

    template<typename Ev>
    event::Button sdlEventToButton(const Ev& ev)
    {
        return ev.button == SDL_BUTTON_LEFT ? event::Button::Left : event::Button::Right;
    }

    int KeyToModifier(const SDL_Keycode& key)
    {
        using namespace awe::ipc;
        switch(key) {
            case SDLK_LCTRL: return keyModifiers::LeftControl;
            case SDLK_LSHIFT: return keyModifiers::LeftShift;
            case SDLK_LALT: return keyModifiers::LeftAlt;
            case SDLK_LGUI: return keyModifiers::LeftGUI;
            case SDLK_RCTRL: return keyModifiers::RightControl;
            case SDLK_RSHIFT: return keyModifiers::RightShift;
            case SDLK_RALT: return keyModifiers::RightAlt;
            case SDLK_RGUI: return keyModifiers::RightGUI;
        }
        return 0;
    }

    std::pair<awe::ipc::KeyCode, int> ConvertSdlKeycode(const SDL_Keycode& key)
    {
        using namespace awe::ipc;
        const auto ch = ((key & SDLK_SCANCODE_MASK) == 0) ? key : 0;
        switch(key) {
            case SDLK_a ... SDLK_z: return { static_cast<KeyCode>(static_cast<int>(KeyCode::A) - (key - SDLK_a)), ch };
            case SDLK_0 ... SDLK_9: return { static_cast<KeyCode>(static_cast<int>(KeyCode::n0) - (key - SDLK_0)), ch };
            case SDLK_F1 ... SDLK_F12: return { static_cast<KeyCode>(static_cast<int>(KeyCode::F1) - (key - SDLK_F1)), ch };
            case SDLK_RETURN: return { KeyCode::Enter, '\r' };
            case SDLK_ESCAPE: return { KeyCode::Escape, 27 };
            case SDLK_SPACE: return { KeyCode::Space, ' ' };
            case SDLK_TAB: return { KeyCode::Tab, '\t' };
            case SDLK_LEFT: return { KeyCode::LeftArrow, 0 };
            case SDLK_RIGHT: return { KeyCode::RightArrow, 0 };
            case SDLK_UP: return { KeyCode::UpArrow, 0 };
            case SDLK_DOWN: return { KeyCode::DownArrow, 0 };
            case SDLK_LCTRL: return { KeyCode::LeftControl, 0 };
            case SDLK_LSHIFT: return { KeyCode::LeftShift, 0 };
            case SDLK_LALT: return { KeyCode::LeftAlt, 0 };
            case SDLK_LGUI: return { KeyCode::LeftGUI, 0 };
            case SDLK_RCTRL: return { KeyCode::RightControl, 0 };
            case SDLK_RSHIFT: return { KeyCode::RightShift, 0 };
            case SDLK_RALT: return { KeyCode::RightAlt, 0 };
            case SDLK_RGUI: return { KeyCode::RightGUI, 0 };
        }
        return { KeyCode::Unknown, ch };
    }

    int ApplyModifiers(const int modifiers, int ch)
    {
        using namespace awe::ipc;
        const auto shiftActive = modifiers & (keyModifiers::LeftShift | keyModifiers::RightShift) != 0;
        const auto controlActive = modifiers & (keyModifiers::LeftControl | keyModifiers::RightControl) != 0;
        if (shiftActive) {
            switch (ch) {
                case 'a' ... 'z': ch = toupper(ch); break;
                case '`': ch = '~'; break;
                case '1': ch = '!'; break;
                case '2': ch = '@'; break;
                case '3': ch = '#'; break;
                case '4': ch = '$'; break;
                case '5': ch = '%'; break;
                case '6': ch = '^'; break;
                case '7': ch = '&'; break;
                case '8': ch = '*'; break;
                case '9': ch = '('; break;
                case '0': ch = ')'; break;
                case '-': ch = '_'; break;
                case '+': ch = '+'; break;
                case '[': ch = '{'; break;
                case ']': ch = '}'; break;
                case '\\': ch = '|'; break;
                case ';': ch = ':'; break;
                case '"': ch = '\''; break;
                case '/': ch = '?'; break;
            }
        }
        if (controlActive) {
            if (toupper(ch) >= 'A' && toupper(ch) <= 'Z') ch = (ch - 'A') + 1;
        }
        return ch;
    }
} // namespace

struct Platform_SDL2::Impl {
    SDL_Window* window{};
    SDL_Renderer* renderer{};
    SDL_Texture* texture{};
    int activeKeyModifiers{};

    Impl()
    {
        SDL_Init(SDL_INIT_VIDEO);
        window = SDL_CreateWindow(
            "AWM", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, size.width, size.height, 0);
        renderer = SDL_CreateRenderer(window, -1, 0);
        texture = SDL_CreateTexture(
            renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, size.width, size.height);
    }

    ~Impl()
    {
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
    }

    void Render(awe::PixelBuffer& fb)
    {
        SDL_UpdateTexture(texture, nullptr, fb.buffer, fb.size.width * sizeof(PixelValue));
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    std::optional<Event> Poll()
    {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT:
                    return event::Quit{};
                case SDL_MOUSEBUTTONDOWN:
                    return event::MouseButtonDown{sdlEventToButton(ev.button),
                                                  {ev.button.x, ev.button.y}};
                case SDL_MOUSEBUTTONUP:
                    return event::MouseButtonUp{sdlEventToButton(ev.button),
                                                {ev.button.x, ev.button.y}};
                case SDL_MOUSEMOTION:
                    return event::MouseMotion{{ev.motion.x, ev.motion.y}};
                case SDL_KEYDOWN: {
                    const auto key = ConvertSdlKeycode(ev.key.keysym.sym);
                    const auto modifier = KeyToModifier(ev.key.keysym.sym);
                    activeKeyModifiers |= modifier;
                    const auto ch = ApplyModifiers(activeKeyModifiers, key.second);
                    return event::KeyDown{key.first, activeKeyModifiers, ch};
                }
                case SDL_KEYUP: {
                    const auto key = ConvertSdlKeycode(ev.key.keysym.sym);
                    const auto modifier = KeyToModifier(ev.key.keysym.sym);
                    activeKeyModifiers &= ~modifier;
                    const auto ch = ApplyModifiers(activeKeyModifiers, key.second);
                    return event::KeyUp{key.first, activeKeyModifiers, ch};
                }
            }
        }
        return {};
    }
};

Platform_SDL2::Platform_SDL2() : impl(std::make_unique<Impl>()) {}

Platform_SDL2::~Platform_SDL2() = default;

void Platform_SDL2::Render(awe::PixelBuffer& fb) { return impl->Render(fb); }

std::optional<Event> Platform_SDL2::Poll() { return impl->Poll(); }

awe::Size Platform_SDL2::GetSize() { return size; }
