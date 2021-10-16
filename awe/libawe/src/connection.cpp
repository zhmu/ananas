#include "awe/connection.h"
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/shm.h>
#include <stdexcept>
#include <cstring>
#include <unistd.h>

namespace awe
{
    Connection::Connection(const char* path)
    {
        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) throw std::runtime_error("cannot create unix domain socket");

        sockaddr_un sun{};
        sun.sun_family = AF_UNIX;
        strcpy(sun.sun_path, path);
        if (connect(fd, reinterpret_cast<sockaddr*>(&sun), sizeof(sun)) < 0)
            throw std::runtime_error("connection failure");
    }

    Connection::~Connection()
    {
        close(fd);
    }

    bool Connection::Read(void* buf, const size_t len)
    {
        return read(fd, buf, len) == len;
    }

    bool Connection::Write(const void* buf, const size_t len)
    {
        return write(fd, buf, len) == len;
    }

    std::optional<ipc::Event> Connection::ReadEvent()
    {
        ipc::Event event;
        auto p = reinterpret_cast<char*>(&event);
        for(auto left = sizeof(event); left > 0; ) {
            const auto n = read(fd, p, left);
            if (n <= 0) return {};
            p += n, left -= n;
        }
        return event;
    }
}
