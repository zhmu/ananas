#include "windowmanager.h"
#include "types.h"
#include "window.h"

#include <cassert>
#include <cstring>
#include <sys/shm.h>

#ifdef __Ananas__
# define FONT_PATH "/usr/share/fonts"
#else
# define FONT_PATH "../data"
#endif

struct WindowManager::EventProcessor {
    WindowManager& wm;
    bool quit{};
    Window* draggingWindow{};
    Point previousPoint{};

    void operator()(const event::Quit&) { quit = true; }

    void operator()(const event::MouseButtonDown& ev)
    {
        if (ev.button == event::Button::Left) {
            if (auto w = wm.FindWindowHeaderAt(ev.position); w) {
                draggingWindow = w;
                previousPoint = ev.position;
                return;
            }
        }
    }

    void operator()(const event::MouseButtonUp& ev)
    {
        if (ev.button == event::Button::Left) {
            draggingWindow = nullptr;
        }
    }

    void operator()(const event::MouseMotion& ev)
    {
        if (draggingWindow) {
            Point delta = ev.position - previousPoint;
            previousPoint = ev.position;
            draggingWindow->position = draggingWindow->position + delta;
        }
    }
};

WindowManager::WindowManager(std::unique_ptr<Platform> platform)
    : pixelBuffer(platform->GetSize()), platform(std::move(platform)),
      font(FONT_PATH "/Roboto-Regular.ttf", 20),
      eventProcessor(std::make_unique<EventProcessor>(*this))
{
}

WindowManager::~WindowManager() = default;

void WindowManager::Update()
{
    pixelBuffer.FilledRectangle({{}, pixelBuffer.size}, palette.backgroundColour);
    for (auto& w : windows) {
        w->Draw(pixelBuffer, font, palette);
    }
}

Window* WindowManager::FindWindowHeaderAt(const Point& p)
{
    for (auto& w : windows) {
        if (w->HitsHeaderRectangle(p))
            return w.get();
    }
    return nullptr;
}

Window& WindowManager::CreateWindow(const Size& size)
{
    auto w = std::make_unique<Window>(Point{100, 100}, size);

    const auto shmDataSize = size.width * size.height * sizeof(PixelValue);
    w->shmId = shmget(IPC_PRIVATE, shmDataSize, IPC_CREAT | 0600);
    assert(w->shmId >= 0);

    auto data = shmat(w->shmId, NULL, 0);
    assert(data != reinterpret_cast<void*>(-1));
    w->shmData = static_cast<uint32_t*>(data);
    memset(w->shmData, 0, shmDataSize);

    windows.push_back(std::move(w));
    return *windows.back();
}

Window* WindowManager::FindWindowByHandle(const Handle handle)
{
    auto it = std::find_if(
        windows.begin(), windows.end(), [&](const auto& w) { return w->handle == handle; });
    return it != windows.end() ? it->get() : nullptr;
}

bool WindowManager::Run()
{
    if (auto event = platform->Poll(); event) {
        std::visit(*eventProcessor, *event);
        needUpdate = true;
    }

    if (needUpdate) {
        Update();
        platform->Render(pixelBuffer);
        needUpdate = false;
    }

    return !eventProcessor->quit;
}
