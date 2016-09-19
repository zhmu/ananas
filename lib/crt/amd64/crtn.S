/*
 * Final object file; this is used to place trailing code after the
 * .init and .fini sections.
 */
.section .init,"ax",@progbits
	addq	$8, %rsp
	ret

.section .fini,"ax",@progbits
	addq	$8, %rsp
	ret
