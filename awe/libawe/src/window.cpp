#include "awe/window.h"
#include <stdexcept>
#include <unistd.h>
#include <cstring>
#include <sys/shm.h>
#include "awe/connection.h"
#include "awe/pixelbuffer.h"

namespace awe
{

Window::Window(Connection& connection, ipc::Handle handle, PixelValue* frameBuffer, Size windowSize)
    : connection(connection), handle(handle), size(std::move(windowSize))
    , pixelBuffer(std::make_unique<PixelBuffer>(frameBuffer, size))
{
}

Window::~Window()
{
    shmdt(pixelBuffer->GetBuffer());
}

void Window::SetTitle(const char* title)
{
    const auto fd = connection.GetFileDescriptor();

    ipc::Message msgSetWindowTitle{ ipc::MessageType::SetWindowTitle };
    msgSetWindowTitle.u.setWindowTitle.handle = handle;
    strncpy(msgSetWindowTitle.u.setWindowTitle.title, title, sizeof(msgSetWindowTitle.u.setWindowTitle.title) - 1);
    if (!connection.Write(msgSetWindowTitle))
        throw std::runtime_error("write");

    ipc::GenericReply reply;
    if (!connection.Read(reply))
        throw std::runtime_error("read");
}

void Window::Invalidate()
{
    const auto fd = connection.GetFileDescriptor();

    ipc::Message msgUpdateWindow{ ipc::MessageType::UpdateWindow };
    msgUpdateWindow.u.updateWindow.handle = handle;
    if (!connection.Write(msgUpdateWindow))
        throw std::runtime_error("write");
}

namespace window
{
    std::unique_ptr<Window> Create(Connection& connection, const Size& size, const char* title)
    {
        const auto fd = connection.GetFileDescriptor();

        awe::ipc::Message msgCreateWindow{ awe::ipc::MessageType::CreateWindow };
        msgCreateWindow.u.createWindow.height = size.height;
        msgCreateWindow.u.createWindow.width = size.width;
        strcpy(msgCreateWindow.u.createWindow.title, title);
        if (!connection.Write(msgCreateWindow))
            throw std::runtime_error("cannot write to socket");

        awe::ipc::CreateWindowReply reply;
        if (!connection.Read(reply))
            throw std::runtime_error("cannot read reply from socket");

        if (reply.result != awe::ipc::ResultCode::Success)
            throw std::runtime_error("server refused to create a window");
        auto handle = reply.handle;

        auto framebuffer = static_cast<PixelValue*>(shmat(reply.fbKey, NULL, 0));
        if (framebuffer == reinterpret_cast<void*>(-1))
            throw std::runtime_error("shmat");
        return std::make_unique<Window>(connection, handle, framebuffer, size);
    }
}

}
