/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __NETDB_H__
#define __NETDB_H__

#include <inttypes.h>
#include <sys/types.h>
#include <sys/cdefs.h>

struct hostent {
    char* h_name;
    char** h_aliases;
    int h_addrtype;
    int h_length;
    char** h_addr_list;
};

struct netent {
    char* n_name;
    char** n_aliases;
    int n_addrtype;
    uint32_t n_net;
};

struct protoent {
    char* p_name;
    char** p_aliases;
    int p_proto;
};

struct servent {
    char* s_name;
    char** s_aliases;
    int s_port;
    char* s_proto;
};

struct addrinfo {
    int ai_flags;
#define AI_PASSIVE 0x0001
#define AI_CANONNAME 0x0002
#define AI_NUMERICHOST 0x0004
#define AI_NUMERICSERV 0x0008
#define AI_V4MAPPED 0x0010
#define AI_ALL 0x0020
#define AI_ADDRCONFIG 0x0040
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    socklen_t ai_addrlen;
    struct sockaddr* ai_addr;
    char* ai_canonname;
    struct addrinfo* ai_next;
};

#define NI_NOFQDN 0x0001
#define NI_NUMERICHOST 0x0002
#define NI_NAMEREQD 0x0004
#define NI_NUMERICSERV 0x0008
#define NI_NUMERICSCOPE 0x0010
#define NI_DGRAM 0x0020

__BEGIN_DECLS

struct servent* getservbyname(const char* name, const char* proto);
struct hostent* gethostbyname(const char* name);

__END_DECLS

#endif /* __NETDB_H__ */
