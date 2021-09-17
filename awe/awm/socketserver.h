#pragma once

#include <functional>
#include <memory>
#include <vector>

class WindowManager;
struct SocketClient;

using FileDescriptor = int;
using ProcessDataFunction = std::function<bool(FileDescriptor)>;

class SocketServer final
{
    ProcessDataFunction processData;

    const FileDescriptor serverFd;
    std::vector<FileDescriptor> clients;

  public:
    SocketServer(ProcessDataFunction, const char* path);
    ~SocketServer();

    void Poll();
};
