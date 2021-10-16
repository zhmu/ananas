#pragma once

#include <memory>
#include "awe/ipc.h"
#include "awe/types.h"

namespace awe
{

class Connection;
class PixelBuffer;

class Window
{
    Connection& connection;
    const ipc::Handle handle;
    Size size;
    std::unique_ptr<PixelBuffer> pixelBuffer;

public:
    Window(Connection& connection, ipc::Handle handle, PixelValue* frameBuffer, Size windowSize);
    ~Window();

    PixelBuffer& GetPixelBuffer() { return *pixelBuffer; }
    void SetTitle(const char*);
    void Invalidate();
};

namespace window
{
    std::unique_ptr<Window> Create(Connection& connection, const Size& size, const char* title);
}

}
