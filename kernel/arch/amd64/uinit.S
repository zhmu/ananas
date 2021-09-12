.data

#define SYSCALL_exit 0
#define SYSCALL_execve 9

.globl initcode
.globl initcode_end

initcode:
        movq        (%rdi), %rdi            /* Init path */

        movq        $SYSCALL_execve, %rax
        leaq        argv(%rip), %rsi
        leaq        envp(%rip), %rdx
        movq        %rdi, (%rsi)
        syscall

        movq        $SYSCALL_exit, %rax
        movq        $-1, %rdi
        syscall

        ud2

argv:   .quad   0
envp:   .quad   0

initcode_end:
