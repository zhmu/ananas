#ifndef UTIME_H
#define UTIME_H

#include <machine/_types.h>
#include <ananas/_types/time.h>

struct utimbuf {
	time_t	actime;
	time_t	modtime;
};

int utime(const char* path, const struct utimbuf* times);

#endif /* UTIME_H */
