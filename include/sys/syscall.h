#include <sys/types.h>

#ifndef __SYSCALL_H__
#define __SYSCALL_H__

struct SYSCALL_ARGS {
	register_t	number;
	register_t	arg1, arg2, arg3, arg4, arg5;
};

register_t syscall(struct SYSCALL_ARGS* args);

#endif /* __SYSCALL_H__ */
