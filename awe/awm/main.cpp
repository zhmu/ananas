#include <cassert>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <variant>
#include <vector>
#include <iostream>

#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "awe/types.h"
#include "awe/font.h"
#include "awe/ipc.h"
#include "awe/pixelbuffer.h"
#ifdef __Ananas__
# include "platform_ananas.h"
using PlatformImpl = Platform_Ananas;
#else
# include <SDL.h>
# include "platform_sdl2.h"
using PlatformImpl = Platform_SDL2;
#endif
#include "palette.h"
#include "window.h"
#include "windowmanager.h"
#include "socketserver.h"

int main(int argc, char* argv[])
{
    try {
        WindowManager wm(std::make_unique<PlatformImpl>());

        {
            auto w = std::make_unique<Window>(awe::Point{20, 50}, awe::Size{100, 150}, 0);
            w->title = "Hello world";
            wm.windows.push_back(std::move(w));
        }
        {
            auto w = std::make_unique<Window>(awe::Point{250, 250}, awe::Size{150, 200}, 0);
            w->title = "Hi there";
            wm.windows.push_back(std::move(w));
        }

        auto processData = [&](int fd) {
            if (fd == wm.platform->GetEventFd()) {
                wm.HandlePlatformEvents();
                return true;
            }

            awe::ipc::Message m;
            const int numRead = read(fd, &m, sizeof(m));
            if (numRead <= 0)
                return false;
            if (numRead != sizeof(m)) {
                std::cerr << "SocketClient: partial read, dropping\n";
                return false;
            }

            switch (m.type) {
                case awe::ipc::MessageType::CreateWindow: {
                    const auto& createWindow = m.u.createWindow;

                    const auto size = awe::Size{static_cast<int>(createWindow.width),
                                                static_cast<int>(createWindow.height)};
                    auto& w = wm.CreateWindow(size, fd);

                    awe::ipc::CreateWindowReply reply;
                    reply.result = awe::ipc::ResultCode::Success;
                    reply.handle = w.handle;
                    reply.fbKey = w.shmId;
                    if (send(fd, &reply, sizeof(reply), 0) != sizeof(reply)) {
                        return false; // TODO clean
                    }
                    return true;
                }
                case awe::ipc::MessageType::UpdateWindow: {
                    auto w = wm.FindWindowByHandle(m.u.updateWindow.handle);

                    if (w != nullptr) {
                        wm.Invalidate(w->GetClientRectangle());
                    return true;
                }
                case awe::ipc::MessageType::SetWindowTitle: {
                    auto w = wm.FindWindowByHandle(m.u.setWindowTitle.handle);

                    awe::ipc::GenericReply reply;
                    if (w != nullptr) {
                        // TODO null check
                        w->title = m.u.setWindowTitle.title;
                        reply.result = awe::ipc::ResultCode::Success;
                    } else {
                        reply.result = awe::ipc::ResultCode::BadHandle;
                    }
                    return send(fd, &reply, sizeof(reply), 0) == sizeof(reply);
                }
            }
            return true;
        };

        auto connectionLost = [&](int fd) {
            while(true) {
                auto w = wm.FindWindowByFd(fd);
                if (!w) break;
                wm.DestroyWindow(*w);
            }
        };

        SocketServer server(std::move(processData), std::move(connectionLost), "/tmp/awewm");
        if (auto eventFd = wm.platform->GetEventFd(); eventFd >= 0)
            server.AddClient(eventFd);

#if 0
        pid_t p = fork();
        if (p == 0) {
            char prog[] = "/usr/games/doom";
            char arg0[] = "-iwad";
            char arg1[] = "/usr/share/games/doom/doom1.wad";
            char* const argv[] = { prog, arg0, arg1, NULL };
            execv(prog, argv);
            exit(-1);
        }
#endif
#if 1
        pid_t q = fork();
        if (q == 0) {
            char prog[] = "/usr/bin/awterm";
            char* const argv[] = { prog, NULL };
            execv(prog, argv);
            exit(-1);
        }
#endif

        signal(SIGPIPE, SIG_IGN);
        while (true) {
            server.Poll();
            if (!wm.Run())
                break;
        }
    } catch (std::exception& e) {
        printf("exception: %s\n", e.what());
        return -1;
    }

    return 0;
}
