.text

.globl usupport_start, usupport_end
/*
 * Everything between usupport_start .. usupport_end will be made available in
 * the userland support page at USERLAND_SUPPORT_ADDR. This is mapped r-x and
 * is intended for signal return code which we don't want to have to look for
 * in libc and such.
 */
usupport_start:
	nop
usupport_end:
