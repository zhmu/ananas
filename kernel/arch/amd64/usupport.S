.text

.globl usupport_start, usupport_end
/*
 * Everything between usupport_start .. usupport_end will be made available in
 * the userland support page at USERLAND_SUPPORT_ADDR. This is mapped r-x and
 * is intended for signal return code which we don't want to have to look for
 * in libc and such.
 */
usupport_start:
    /*
     * sigret trampoline is made available to userspace to avoid any ABI
     * requirements.
     */
    movq    $30, %rax   /* XXX should be from syscalls.h */
    syscall
    /* NOTREACHED */

    nop
usupport_end:
