#ifndef SYS_TIMES_H
#define SYS_TIMES_H

#include <ananas/types.h>
#include <sys/cdefs.h>

struct tms {
    clock_t tms_utime;
    clock_t tms_stime;
    clock_t tms_cutime;
    clock_t tms_cstime;
};

__BEGIN_DECLS

clock_t times(struct tms* buffer);

__END_DECLS

#endif /* SYS_TIMES_H */
