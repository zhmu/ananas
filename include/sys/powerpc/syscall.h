#ifndef __MD_SYSCALL_H__
#define __MD_SYSCALL_H__

#define SYSCALL0(x) SYSCALL(x)
#define SYSCALL1(x) SYSCALL(x)
#define SYSCALL2(x) SYSCALL(x)
#define SYSCALL3(x) SYSCALL(x)
#define SYSCALL4(x) SYSCALL(x)
#define SYSCALL5(x) SYSCALL(x)
#define SYSCALL6(x) SYSCALL(x)

/*
 * Userland and kernel calling conventions are identical, so we can just
 * hook to the systemm call instruction.
 */
#define SYSCALL(x) 		\
	__asm(			\
		"sc\n"		\
	: : "a" (x));

#endif /* __MD_SYSCALL_H__ */
