#include "socketserver.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <unistd.h>
#include <cstring>

SocketServer::SocketServer(ProcessDataFunction processData, OnConnectionTerminated connectionLost, const char* path)
    : processData(std::move(processData)), connectionLost(std::move(connectionLost)), serverFd(socket(AF_UNIX, SOCK_STREAM, 0))
{
    if (serverFd < 0)
        throw std::runtime_error("cannot create socket");

    unlink(path);
    struct sockaddr_un sun;
    sun.sun_family = AF_UNIX;
    strncpy(sun.sun_path, path, sizeof(sun.sun_path) - 1);
    if (bind(serverFd, reinterpret_cast<struct sockaddr*>(&sun), sizeof(sun)) < 0)
        throw std::runtime_error("cannot bind socket");

    if (listen(serverFd, 5) < 0)
        throw std::runtime_error("cannot listen socket");
}

SocketServer::~SocketServer()
{
    for (auto clientFd : clients)
        close(clientFd);
    close(serverFd);
}

void SocketServer::Poll()
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(serverFd, &fds);

    int nfds = serverFd;
    for (const auto& clientFd : clients) {
        FD_SET(clientFd, &fds);
        nfds = std::max(nfds, clientFd);
    }

    struct timeval tv{};
    auto n = select(nfds + 1, &fds, nullptr, nullptr, &tv);
    if (n <= 0)
        return;

    if (FD_ISSET(serverFd, &fds)) {
        if (auto clientFd = accept(serverFd, nullptr, nullptr); clientFd >= 0) {
            clients.push_back(clientFd);
        }
        --n;
    }

    for (auto clientIterator = clients.begin(); clientIterator != clients.end();) {
        auto clientFd = *clientIterator;
        if (!FD_ISSET(clientFd, &fds)) {
            ++clientIterator;
            continue;
        }

        if (!std::invoke(processData, clientFd)) {
            clients.erase(clientIterator);
            connectionLost(clientFd);
        } else {
            ++clientIterator;
        }
        if (--n == 0)
            break;
    }
}
