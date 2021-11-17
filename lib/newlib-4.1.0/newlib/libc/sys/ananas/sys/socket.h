#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#include <ananas/socket.h>
#include <sys/types.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

int accept(int socket, struct sockaddr* address, socklen_t* address_len);
int bind(int socket, const struct sockaddr* address, socklen_t address_len);
int connect(int socket, const struct sockaddr* address, socklen_t address_len);
int listen(int socket, int backlog);
int socket(int domain, int type, int protocol);

ssize_t send(int socket, const void* buffer, size_t length, int flags);

__END_DECLS

#endif
