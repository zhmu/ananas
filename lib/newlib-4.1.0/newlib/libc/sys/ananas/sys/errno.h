#ifndef _SYS_ERRNO_H_
#define _SYS_ERRNO_H_

#include <ananas/errno.h>

#include <sys/cdefs.h>
#include <sys/reent.h>

#ifndef _REENT_ONLY
#define errno (*__errno())

__BEGIN_DECLS

extern int *__errno (void);

__END_DECLS

#endif

#define __errno_r(ptr) ((ptr)->_errno)

#endif
