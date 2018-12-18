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
 * Userland and kernel calling conventions are almost identical; the only
 * difference is that %rcx is used by syscall/sysret, so we must pass that
 * variable in %r10.
 */
#define SYSCALL(x)              \
    __asm("movq	%%rcx, %%r10\n" \
          "syscall\n"           \
          :                     \
          : "a"(x));

#endif /* __MD_SYSCALL_H__ */
