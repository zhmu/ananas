#pragma once

#include <ananas/select.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* errorfds, struct timeval* timeout);

__END_DECLS
