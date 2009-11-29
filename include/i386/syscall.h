#ifndef __MD_SYSCALL_H__
#define __MD_SYSCALL_H__

/*
 * XXX kludge: the code generated for each syscall sets up a stackframe,
 * because the compiler expects that it's a normal, well-behaving function.
 *
 * We just throw it away and chain to the syscallX function with the correct
 * argument count. These functions return with an ordinary 'ret', since the
 * stackframe is already gone.
 *
 * This chews up at least 2 bytes (leave/ret) per function and it's quite a
 * kludge - should look into telling gcc to stop creating the stackframe XXX
 */
#define SYSCALL0(x) __asm("leave; jmp syscall0" : : "a" (x))
#define SYSCALL1(x) __asm("leave; jmp syscall1" : : "a" (x))
#define SYSCALL2(x) __asm("leave; jmp syscall2" : : "a" (x))
#define SYSCALL3(x) __asm("leave; jmp syscall3" : : "a" (x))
#define SYSCALL4(x) __asm("leave; jmp syscall4" : : "a" (x))
#define SYSCALL5(x) __asm("leave; jmp syscall5" : : "a" (x))

#endif /* __MD_SYSCALL_H__ */
