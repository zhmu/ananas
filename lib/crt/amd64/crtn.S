/*
 * Final object file; this is used to place trailing code after the
 * .init and .fini sections.
 */
.section .init,"ax",@progbits
	ret

.section .fini,"ax",@progbits
	ret
