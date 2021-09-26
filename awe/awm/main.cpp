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

#include "types.h"
#include "font.h"
#include "ipc.h"
#include "palette.h"
#include "pixelbuffer.h"
#ifdef __Ananas__
# include "platform_ananas.h"
using PlatformImpl = Platform_Ananas;
#else
# include <SDL.h>
# include "platform_sdl2.h"
using PlatformImpl = Platform_SDL2;
#endif
#include "window.h"
#include "windowmanager.h"
#include "socketserver.h"

int main(int argc, char* argv[])
{
    try {
        WindowManager wm(std::make_unique<PlatformImpl>());

        {
            auto w = std::make_unique<Window>(Point{20, 50}, Size{100, 150}, 0);
            w->title = "Hello world";
            wm.windows.push_back(std::move(w));
        }
        {
            auto w = std::make_unique<Window>(Point{250, 250}, Size{150, 200}, 0);
            w->title = "Hi there";
            wm.windows.push_back(std::move(w));
        }

        auto processData = [&](int fd) {
            ipc::Message m;
            const int numRead = read(fd, &m, sizeof(m));
            if (numRead <= 0)
                return false;
            if (numRead != sizeof(m)) {
                std::cerr << "SocketClient: partial read, dropping\n";
                return false;
            }

            switch (m.type) {
                case ipc::MessageType::CreateWindow: {
                    const auto& createWindow = m.u.createWindow;

                    const auto size = Size{static_cast<int>(createWindow.width),
                                           static_cast<int>(createWindow.height)};
                    auto& w = wm.CreateWindow(size, fd);

                    ipc::CreateWindowReply reply;
                    reply.result = ipc::ResultCode::Success;
                    reply.handle = w.handle;
                    reply.fbKey = w.shmId;
                    if (send(fd, &reply, sizeof(reply), 0) != sizeof(reply)) {
                        return false; // TODO clean
                    }
                    return true;
                }
                case ipc::MessageType::UpdateWindow: {
                    auto w = wm.FindWindowByHandle(m.u.updateWindow.handle);

                    ipc::GenericReply reply;
                    if (w != nullptr) {
                        reply.result = ipc::ResultCode::Success;
                        wm.needUpdate = true;
                    } else {
                        reply.result = ipc::ResultCode::BadHandle;
                    }
                    return send(fd, &reply, sizeof(reply), 0) == sizeof(reply);
                }
                case ipc::MessageType::SetWindowTitle: {
                    auto w = wm.FindWindowByHandle(m.u.setWindowTitle.handle);

                    ipc::GenericReply reply;
                    if (w != nullptr) {
                        // TODO null check
                        w->title = m.u.setWindowTitle.title;
                        reply.result = ipc::ResultCode::Success;
                    } else {
                        reply.result = ipc::ResultCode::BadHandle;
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
