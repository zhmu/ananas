.data

#define SYSCALL_exit 0
#define SYSCALL_write 2
#define SYSCALL_execve 9

.globl initcode
.globl initcode_end

initcode:
        movq        (%rdi), %rdi            /* Init path */

        xorq        %rax, %rax
        pushq       %rax
        movq        %rsp, %rdx              /* envp[0] = argv[1] = 0 */

        pushq       %rdi
        movq        %rsp, %rsi              /* argv[0] = init path */

        movq        $SYSCALL_execve, %rax
        syscall

        movq        $SYSCALL_write, %rax
        movq        $2, %rdi                    /* stderr */
        leaq        error_msg(%rip), %rsi
        movq        $error_msg_len, %rdx
        syscall

        movq        $SYSCALL_exit, %rax
        movq        $-1, %rdi
        syscall

        ud2

argv:   .quad   0
envp:   .quad   0
error_msg:  .ascii  "FAILURE: Unable to execve() init\n"
error_msg_len =  . - error_msg

initcode_end:
