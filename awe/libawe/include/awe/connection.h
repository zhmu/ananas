#pragma once

#include <optional>
#include "ipc.h"

namespace awe
{

static inline constexpr auto DefaultSocketPath = "/tmp/awewm";

class Connection
{
    int fd{-1};

public:
    Connection(const char* path = DefaultSocketPath);
    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    int GetFileDescriptor() const { return fd; }
    std::optional<ipc::Event> ReadEvent();

    bool Read(void* buf, size_t len);
    bool Write(const void* buf, size_t len);

    template<typename T>
    bool Read(T& buf) {
        return Read(reinterpret_cast<void*>(&buf), sizeof(buf));
    }

    template<typename T>
    bool Write(const T& buf) {
        return Write(reinterpret_cast<const void*>(&buf), sizeof(buf));
    }
};

}
