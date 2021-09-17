#include "platform_sdl2.h"
#include <SDL.h>
#include "pixelbuffer.h"

namespace
{
    inline constexpr Size size{1024, 768};

    template<typename Ev>
    event::Button sdlEventToButton(const Ev& ev)
    {
        return ev.button == SDL_BUTTON_LEFT ? event::Button::Left : event::Button::Right;
    }
} // namespace

struct Platform_SDL2::Impl {
    SDL_Window* window{};
    SDL_Renderer* renderer{};
    SDL_Texture* texture{};

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

    void Render(PixelBuffer& fb)
    {
        SDL_UpdateTexture(texture, nullptr, fb.buffer.get(), fb.size.width * sizeof(PixelValue));
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
            }
        }
        return {};
    }
};

Platform_SDL2::Platform_SDL2() : impl(std::make_unique<Impl>()) {}

Platform_SDL2::~Platform_SDL2() = default;

void Platform_SDL2::Render(PixelBuffer& fb) { return impl->Render(fb); }

std::optional<Event> Platform_SDL2::Poll() { return impl->Poll(); }

Size Platform_SDL2::GetSize() { return size; }
