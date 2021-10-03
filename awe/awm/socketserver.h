#pragma once

#include <functional>
#include <memory>
#include <vector>

class WindowManager;
struct SocketClient;

using FileDescriptor = int;
using ProcessDataFunction = std::function<bool(FileDescriptor)>;
using OnConnectionTerminated = std::function<void(FileDescriptor)>;

class SocketServer final
{
    ProcessDataFunction processData;
    OnConnectionTerminated connectionLost;

    const FileDescriptor serverFd;
    std::vector<FileDescriptor> clients;

  public:
    SocketServer(ProcessDataFunction, OnConnectionTerminated, const char* path);
    ~SocketServer();

    void AddClient(FileDescriptor fd);
    void Poll();
};
