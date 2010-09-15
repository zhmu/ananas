#include <sys/types.h>

#ifndef __SYS_SOCKET_H__
#define __SYS_SOCKET_H__

#include <sys/_types/socklen.h>

#define SOCK_DGRAM	0
#define SOCK_RAW	1
#define SOCK_SEQPACKET	2
#define SOCK_STREAM	3

typedef unsigned int	sa_family_t;

struct sockaddr {
	sa_family_t	sa_family;
	char		sa_data[];
};

#endif /* __SYS_SOCKET_H__ */
