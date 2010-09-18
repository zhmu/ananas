#ifndef __UTIME_H__
#define __UTIME_H__

#include <ananas/types.h>
#include <ananas/_types/time.h>

struct utimbuf {
	time_t	actime;
	time_t	modtime;
};

int utime(const char*, const struct utimbuf*);

#endif /* __UTIME_H__ */
