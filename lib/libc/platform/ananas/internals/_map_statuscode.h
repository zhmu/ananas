#ifndef _MAP_STATUSCODE_H_
#define _MAP_STATUSCODE_H_

#include <ananas/statuscode.h>
#include <errno.h>

static inline int
map_statuscode(statuscode_t status)
{
	if (status != ananas_statuscode_success()) {
		errno = ananas_statuscode_extract_errno(status);
		return -1;
	}

	return 0;
}

#endif /* _MAP_STATUSCODE_H_ */
