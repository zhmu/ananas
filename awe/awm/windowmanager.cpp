#include "windowmanager.h"
#include "awe/types.h"
#include "awe/ipc.h"
#include "window.h"
#include "wmmenu.h"

#include <cassert>
#include <cstring>
#include <sys/shm.h>
#include <sys/socket.h>

#include "mouse.h"

#ifdef __Ananas__
# define FONT_PATH "/usr/share/fonts"
#else
# define FONT_PATH "../data"
#endif

#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>

#define PAGE_SIZE 4096 // XXX

void debug_trace(const char* fmt, ...)
{
    static int fd = -1;
    if (fd < 0) {
        fd = open("/dev/debugcon", O_RDWR);
        if (fd < 0)
            return;
    }

    char s[128];
    va_list va;
    va_start(va, fmt);
    vsnprintf(s, sizeof(s), fmt, va);
    s[sizeof(s) - 1] = '\0';
    va_end(va);

    write(fd, s, strlen(s));
}

namespace
{
    constexpr inline auto fontHeight = 20;

    void Launch(const char* progname, const std::vector<std::string>& args)
    {
        pid_t p = fork();
        if (p == 0) {
            char** argv = new char*[args.size() + 2];
            size_t n = 0;
            argv[n] = strdup(progname);
            for (; n < args.size(); ++n) {
                argv[n + 1] = strdup(args[n].c_str());
            }
            argv[n + 1] = nullptr;
            execv(strdup(progname), argv);
            exit(-1);
        }
    }

    const std::vector wmItems{
        WMItem{ "awterm", []() { Launch("/usr/bin/awterm", { } ); }},
        WMItem{ "DOOM", []() { Launch("/usr/bin/doom", { "-iwad", "/usr/share/games/doom/doom1.wad" } ); }},
    };
}

struct WindowManager::EventProcessor {
    WindowManager& wm;
    bool quit{};
    Window* draggingWindow{};
    Window* focusWindow{};
    awe::Point previousPoint{};

    std::unique_ptr<WMMenu> wmMenu;

    void operator()(const event::Quit&) { quit = true; }

    void ChangeFocus(WindowManager& wm, Window* newWindow)
    {
        if (focusWindow) {
            wm.Invalidate(focusWindow->GetFrameRectangle());
            focusWindow->focus = false;
        }
        focusWindow = newWindow;
        if (focusWindow) {
            wm.Invalidate(focusWindow->GetFrameRectangle());
            focusWindow->focus = true;
        }
    }

    void operator()(const event::MouseButtonDown& ev)
    {
        if (ev.button == event::Button::Left) {
            if (auto w = wm.FindWindowAt(ev.position); w) {
                if (wmMenu && &wmMenu->GetWindow() == w) {
                    wmMenu->OnWMMouseLeftButtonDown(ev.position);
                    wmMenu.reset();
                    return;
                }
                ChangeFocus(wm, w);
                if (w->HitsHeaderRectangle(ev.position)) {
                    draggingWindow = w;
                    previousPoint = ev.position;
                }
            } else {
                ChangeFocus(wm, nullptr);
            }
        }

        if (ev.button == event::Button::Right) {
            if (wm.FindWindowAt(ev.position))
                return;

            if (!wmMenu) {
                wmMenu = std::make_unique<WMMenu>(wm, wmItems, ev.position);
            } else {
                wmMenu.reset();
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
        // Ensure the mouse cursor will get drawn
        wm.Invalidate({ ev.position, { mouse::cursorWidth, mouse::cursorHeight } });

        if (wmMenu) {
            if (wm.FindWindowAt(ev.position) == &wmMenu->GetWindow()) {
                wmMenu->OnWMWindowMouseMotion(ev.position);
            } else {
                wmMenu.reset();
            }
            return;
        }

        if (draggingWindow) {
            wm.Invalidate(draggingWindow->GetFrameRectangle());
            const auto delta = ev.position - previousPoint;
            previousPoint = ev.position;
            draggingWindow->position = draggingWindow->position + delta;
            wm.Invalidate(draggingWindow->GetFrameRectangle());
        }
    }

    void operator()(const event::KeyDown& ev)
    {
        if (ev.key == awe::ipc::KeyCode::Tab && ev.mods == awe::ipc::keyModifiers::LeftAlt) {
            wm.CycleFocus();
            return;
        }

        if (focusWindow) {
            awe::ipc::Event event;
            event.type = awe::ipc::EventType::KeyDown;
            event.handle = focusWindow->handle;
            event.u.keyDown.key = ev.key;
            event.u.keyDown.ch = ev.ch;
            if (send(focusWindow->fd, &event, sizeof(event), 0) != sizeof(event)) {
                printf("send error to fd %d\n", focusWindow->fd);
            }
        }
    }

    void operator()(const event::KeyUp& ev)
    {
        if (focusWindow) {
            awe::ipc::Event event;
            event.type = awe::ipc::EventType::KeyUp;
            event.handle = focusWindow->handle;
            event.u.keyDown.key = ev.key;
            event.u.keyDown.ch = ev.ch;
            if (send(focusWindow->fd, &event, sizeof(event), 0) != sizeof(event)) {
                printf("send error to fd %d\n", focusWindow->fd);
            }
        }
    }
};

WindowManager::WindowManager(std::unique_ptr<Platform> platform)
    : pixelBuffer(platform->GetSize()), platform(std::move(platform)),
      font(FONT_PATH "/Roboto-Regular.ttf", fontHeight),
      eventProcessor(std::make_unique<EventProcessor>(*this)),
      needUpdate{ {}, platform->GetSize() }
{
}

WindowManager::~WindowManager() = default;

void WindowManager::Update()
{
    pixelBuffer.FilledRectangle(needUpdate, palette.backgroundColour);
    for (auto& w : windows) {
        const auto& frameRect = w->GetFrameRectangle();
        const auto rectToUpdate = Intersect(needUpdate, frameRect);
        if (rectToUpdate.empty()) continue;
        w->Draw(pixelBuffer, font, palette);
    }
}

Window* WindowManager::FindWindowAt(const awe::Point& p)
{
    const auto it = std::find_if(
        windows.begin(), windows.end(), [&](const auto& w) { return w->HitsRectangle(p); });
    return it != windows.end() ? it->get() : nullptr;
}

Window& WindowManager::CreateWindow(const awe::Point& pos, const awe::Size& size, int fd)
{
    auto w = std::make_unique<Window>(pos, size, fd);

    auto shmDataSize = size.width * size.height * sizeof(awe::PixelValue);
    if (auto roundUp = shmDataSize % PAGE_SIZE; roundUp > 0)
        shmDataSize += PAGE_SIZE - roundUp;
    w->shmId = shmget(IPC_PRIVATE, shmDataSize, IPC_CREAT | 0600);
    assert(w->shmId >= 0);

    auto data = shmat(w->shmId, NULL, 0);
    assert(data != reinterpret_cast<void*>(-1));
    w->shmData = static_cast<awe::PixelValue*>(data);
    memset(w->shmData, 0, shmDataSize);
    Invalidate(w->GetFrameRectangle());

    windows.push_back(std::move(w));
    eventProcessor->ChangeFocus(*this, windows.back().get()); // XXX annoying
    return *windows.back();
}

Window* WindowManager::FindWindowByHandle(const awe::Handle handle)
{
    const auto it = std::find_if(
        windows.begin(), windows.end(), [&](const auto& w) { return w->handle == handle; });
    return it != windows.end() ? it->get() : nullptr;
}

Window* WindowManager::FindWindowByFd(const int fd)
{
    const auto it = std::find_if(
        windows.begin(), windows.end(), [&](const auto& w) { return w->fd == fd; });
    return it != windows.end() ? it->get() : nullptr;
}

void WindowManager::HandlePlatformEvents()
{
    while (true) {
        const auto event = platform->Poll();
        if (!event) break;
        std::visit(*eventProcessor, *event);
    }
}

bool WindowManager::Run()
{
    if (platform->GetEventFd() < 0) {
        HandlePlatformEvents();
    }

    if (!needUpdate.empty()) {
        Update();
        platform->Render(pixelBuffer, needUpdate);
        needUpdate = awe::Rectangle{};
    }

    return !eventProcessor->quit;
}

void WindowManager::DestroyWindow(Window& w)
{
    const auto it = std::find_if(windows.begin(), windows.end(), [&](auto& p) { return p.get() == &w; });
    assert(it != windows.end());

    Invalidate((*it)->GetFrameRectangle());
    windows.erase(it);
}

void WindowManager::CycleFocus()
{
    auto it = std::find_if(windows.begin(), windows.end(), [&](auto& p) { return p.get() == eventProcessor->focusWindow; });
    if (it != windows.end()) {
        ++it;
        if (it == windows.end())
            it = windows.begin();
    } else {
        it = windows.begin();
    }

    eventProcessor->ChangeFocus(*this, it->get());
}

void WindowManager::Invalidate(const awe::Rectangle& r)
{
    needUpdate = Combine(needUpdate, r);
}

awe::Point WindowManager::DeterminePositionFor(const awe::Size&) const
{
    // TODO Improve this
    return awe::Point{100, 100};
}
