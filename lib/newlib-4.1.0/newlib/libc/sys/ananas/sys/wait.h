#pragma once

#include <ananas/wait.h>
#include <sys/cdefs.h>
#include <sys/types.h>

__BEGIN_DECLS

pid_t wait (int *);
pid_t waitpid (pid_t, int *, int);

__END_DECLS
