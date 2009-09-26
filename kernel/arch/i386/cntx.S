.text

#include "param.h"
#include "types.h"

.global md_store_ctx, md_restore_ctx

/*
 * Stores the current thread context. This function expects to be
 * called from an interrupt context, as a near call.
 */
md_store_ctx:
	/*
	 * Start by saving some registers; this must be done because
	 * we need to setup the per-cpu segment, data segment (we run
	 * in kernel mode) - we use %ebx as a reference register, so
	 * we must store it well.
	 */
	pushl	%ebx
	pushl	%fs
	pushl	%ds
	movw	$0x28, %bx
	movw	%bx, %fs

	/* Setup ds so we can store the registers */
	movw	$0x10, %bx
	movw	%bx, %ds

	/* Fetch the address from the context from the per-cpu info */
	movl	%fs:0, %ebx

	/* Store the current registers - we have to do %ebx last */
	movl	%eax, 0x00(%ebx)
	movl	%ecx, 0x08(%ebx)
	movl	%edx, 0x0c(%ebx)
	movl	%esi, 0x10(%ebx)
	movl	%edi, 0x14(%ebx)
	movl	%ebp, 0x18(%ebx)
	movl	%esp, 0x1c(%ebx)
	/*
	 * The %esp we just stored isn't correct, as we're a variable
	 * and a stackframe further. Current stack looks like this
	 * (note that pushl does esp-4, so we need to add things back
	 * to it to obtain the data!)
	 *
	 *      + 28 - original stack
	 *      + 24 - interrupt: saved eflags
	 *      + 20 - interrupt: original cs
	 *      + 16 - interrupt: original eip
	 *      + 12 - caller: return eip
	 *      +  8 - original ebx
	 *      +  4 - original fs
	 * %esp +  0 - original ds
	 */
	addl	$28, 0x1c(%ebx)		/* update esp */
	xor	%eax, %eax
	movw	%es,  %ax
	movl	%eax,  0x2c(%ebx)
	movw	%gs,  %ax
	movl	%eax,  0x34(%ebx)
	movw	%ss,  %ax
	movl	%eax,  0x38(%ebx)
	movl	%cr3, %eax
	movl	%eax, 0x3c(%ebx)

	/* Store original ds */
	popl	%eax
	movl	%eax,  0x28(%ebx)

	/* Store original fs */
	popl	%eax
	movl	%eax,  0x30(%ebx)

	/* Store ebx */
	movl	%ebx, %edx
	popl	%ebx
	movl	%ebx, 0x04(%edx)

	/*
	 * Store cs:eip and eflags - we grab those from the interrupt context.
	 * Note that these are 12 earlier (i.e. eip is at esp+4 instead of
	 * esp+16) because we have pop'ed %ebx/fs/ds a few lines back.
	 */
	movl	4(%esp), %eax
	movl	%eax, 0x20(%edx)
	movl	8(%esp), %eax
	movl	%eax, 0x24(%edx)
	movl	12(%esp), %eax
	movl	%eax, 0x40(%edx)

	ret

/*
 * Restores the thread context and jumps to the thread
 *
 * 4(%ebp) = ctx
 */
md_restore_ctx:
	movl	4(%esp), %ebx

	/*
	 * First of all, restore the segment registers, pagetable and
	 * flags; this clobbers %eax. Note that we can restore the
	 * flags here because the flags will not be affected by any
	 * instruction we use.
	 */
	movl	0x28(%ebx), %eax
	movw	%ax, %ds
	movl	0x2c(%ebx), %eax
	movw	%ax, %es
	movl	0x30(%ebx), %eax
	movw	%ax, %fs
	movl	0x34(%ebx), %eax
	movw	%ax, %gs
	movl	0x38(%ebx), %eax
	movw	%ax, %ss
	movl	0x3c(%ebx), %eax
	movl	%eax, %cr3

	/* Restore the standard registers as far as we can */
	movl	0x08(%ebx), %ecx
	movl	0x0c(%ebx), %edx
	movl	0x10(%ebx), %esi
	movl	0x14(%ebx), %edi
	movl	0x18(%ebx), %ebp
	movl	0x1c(%ebx), %esp

	/*
	 * Restore flags; we need a popfl for that, which we can only do now
	 * because ss:esp is initialized.
	 */
	movl	0x40(%ebx), %eax
	push	%eax
	popfl

	/* Restore eax */
	movl	0x00(%ebx), %eax

	/*
	 * OK, almost there; put cs:eip from the context on the stack so we can
	 * return far into them.
	 */
	pushl	0x24(%ebx)	/* cs */
	pushl	0x20(%ebx)	/* eip */

	/* Finally, restore %ebx and go */
	movl	0x04(%ebx), %ebx
	lret
