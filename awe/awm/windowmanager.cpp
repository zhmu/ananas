#include "windowmanager.h"
#include "types.h"
#include "window.h"
#include "ipc.h"

#include <cassert>
#include <cstring>
#include <sys/shm.h>
#include <sys/socket.h>

#ifdef __Ananas__
# define FONT_PATH "/usr/share/fonts"
#else
# define FONT_PATH "../data"
#endif

struct WindowManager::EventProcessor {
    WindowManager& wm;
    bool quit{};
    Window* draggingWindow{};
    Window* focusWindow{};
    Point previousPoint{};

    bool operator()(const event::Quit&) { quit = true; return false; }

    void ChangeFocus(Window* newWindow)
    {
        if (focusWindow) {
            focusWindow->focus = false;
        }
        focusWindow = newWindow;
        if (focusWindow) {
            focusWindow->focus = true;
        }
    }

    bool operator()(const event::MouseButtonDown& ev)
    {
        if (ev.button == event::Button::Left) {
            if (auto w = wm.FindWindowAt(ev.position); w) {
                ChangeFocus(w);
                if (w->HitsHeaderRectangle(ev.position)) {
                    draggingWindow = w;
                    previousPoint = ev.position;
                }
                return true;
            } else {
                ChangeFocus(nullptr);
                return true;
            }
        }
        return false;
    }

    bool operator()(const event::MouseButtonUp& ev)
    {
        if (ev.button == event::Button::Left) {
            draggingWindow = nullptr;
            return true;
        }
        return false;
    }

    bool operator()(const event::MouseMotion& ev)
    {
        if (draggingWindow) {
            Point delta = ev.position - previousPoint;
            previousPoint = ev.position;
            draggingWindow->position = draggingWindow->position + delta;
            return true;
        }
        return false;
    }

    bool operator()(const event::KeyDown& ev)
    {
        if (focusWindow) {
            ipc::Event event;
            event.type = ipc::EventType::KeyDown;
            event.handle = focusWindow->handle;
            event.u.keyDown.key = ev.key;
            event.u.keyDown.ch = ev.ch;
            if (send(focusWindow->fd, &event, sizeof(event), 0) != sizeof(event)) {
                printf("send error to fd %d\n", focusWindow->fd);
            }
        }
        return false;
    }

    bool operator()(const event::KeyUp& ev)
    {
        if (focusWindow) {
            ipc::Event event;
            event.type = ipc::EventType::KeyUp;
            event.handle = focusWindow->handle;
            event.u.keyDown.key = ev.key;
            event.u.keyDown.ch = ev.ch;
            if (send(focusWindow->fd, &event, sizeof(event), 0) != sizeof(event)) {
                printf("send error to fd %d\n", focusWindow->fd);
            }
        }
        return false;
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

Window* WindowManager::FindWindowAt(const Point& p)
{
    for (auto& w : windows) {
        if (w->HitsRectangle(p))
            return w.get();
    }
    return nullptr;
}

Window& WindowManager::CreateWindow(const Size& size, int fd)
{
    auto w = std::make_unique<Window>(Point{100, 100}, size, fd);

    const auto shmDataSize = size.width * size.height * sizeof(PixelValue);
    w->shmId = shmget(IPC_PRIVATE, shmDataSize, IPC_CREAT | 0600);
    assert(w->shmId >= 0);

    auto data = shmat(w->shmId, NULL, 0);
    assert(data != reinterpret_cast<void*>(-1));
    w->shmData = static_cast<uint32_t*>(data);
    memset(w->shmData, 0, shmDataSize);

    windows.push_back(std::move(w));
    eventProcessor->ChangeFocus(windows.back().get()); // XXX annoying
    return *windows.back();
}

Window* WindowManager::FindWindowByHandle(const Handle handle)
{
    auto it = std::find_if(
        windows.begin(), windows.end(), [&](const auto& w) { return w->handle == handle; });
    return it != windows.end() ? it->get() : nullptr;
}

Window* WindowManager::FindWindowByFd(const int fd)
{
    auto it = std::find_if(
        windows.begin(), windows.end(), [&](const auto& w) { return w->fd == fd; });
    return it != windows.end() ? it->get() : nullptr;
}

void WindowManager::HandlePlatformEvents()
{
    while (true) {
        const auto event = platform->Poll();
        if (!event) break;
        needUpdate = std::visit(*eventProcessor, *event) || needUpdate;
    }
}

bool WindowManager::Run()
{
    if (platform->GetEventFd() < 0) {
        HandlePlatformEvents();
    }

    if (needUpdate) {
        Update();
        platform->Render(pixelBuffer);
        needUpdate = false;
    }

    return !eventProcessor->quit;
}

void WindowManager::DestroyWindow(Window& w)
{
    auto it = std::find_if(windows.begin(), windows.end(), [&](auto& p) { return p.get() == &w; });
    assert(it != windows.end());

    windows.erase(it);
    needUpdate = true;
}
