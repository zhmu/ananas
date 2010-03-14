/*
 * Low-level system call code.
 *
 */
.text

.global syscall0, syscall1, syscall2, syscall3, syscall4, syscall5

syscall5:
syscall4:
syscall3:
syscall2:
syscall1:
syscall0:
	syscall
	ret
