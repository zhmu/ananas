/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <sys/types.h>
#include <sys/cdefs.h>

#define AF_INET 1
#define AF_LOCAL 2
#define AF_UNDEFINED 3
#define AF_INET6 4

#define AF_UNIX AF_LOCAL

#define SOCK_DGRAM 0
#define SOCK_RAW 1
#define SOCK_SEQPACKET 2
#define SOCK_STREAM 3

typedef unsigned int sa_family_t;

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[];
};

#define SOMAXCONN 1

__BEGIN_DECLS

int accept(int socket, struct sockaddr* address, socklen_t* address_len);
int bind(int socket, const struct sockaddr* address, socklen_t address_len);
int connect(int socket, const struct sockaddr* address, socklen_t address_len);
int listen(int socket, int backlog);
int socket(int domain, int type, int protocol);

ssize_t send(int socket, const void* buffer, size_t length, int flags);

__END_DECLS
