#ifndef _POSIX_TODO_H_
#define _POSIX_TODO_H_

#include <stdio.h>	/* for printf() */
#include <errno.h>	/* for errno :-) */

#define TODO 								\
	do {								\
		printf("%s: not implemented yet\n", __func__); 		\
		errno = ENOSYS;						\
	} while (0)

#endif /* _POSIX_TODO_H_ */
