.text

#include "param.h"
#include "types.h"

.global md_store_ctx, md_restore_ctx

/*
 * Stores the current thread context. This function expects to be
 * called from an interrupt context, as a near call, with the
 * context pointer on the stack.
 */
md_store_ctx:
	pushl	%ebx		/* Store ebx for later */
	movl	8(%esp), %ebx

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
	 *      + 24 - interrupt: saved eflags
	 *      + 20 - interrupt: original cs
	 *      + 16 - interrupt: original eip
	 *      + 12  - caller: return eip
	 *      + 8 - (currentctx)
	 *      + 4 - %ebx
	 * %esp + 0 - return eip
	 */
	addl	$24, 0x1c(%ebx)		/* update esp */
	movw	%ds,  %ax
	movw	%ax,  0x28(%ebx)
	movw	%es,  %ax
	movw	%ax,  0x2c(%ebx)
	movw	%fs,  %ax
	movw	%ax,  0x30(%ebx)
	movw	%gs,  %ax
	movw	%ax,  0x34(%ebx)
	movw	%ss,  %ax
	movw	%ax,  0x38(%ebx)
	movl	%cr3, %eax
	movl	%eax, 0x3c(%ebx)

	/* Store ebx */
	movl	%ebx, %edx
	popl	%ebx
	movl	%ebx, 0x04(%edx)

	/*
	 * Store cs:eip and eflags - we grab those from the interrupt context. Note
	 * that these are 4 earlier (i.e. eip is at esp+8 instead of esp+12) becaus
	 * we have pop'ed %ebx a few lines back.
	 */
	movl	8(%esp), %eax
	movl	%eax, 0x20(%edx)
	movl	12(%esp), %eax
	movl	%eax, 0x24(%edx)
	movl	16(%esp), %eax
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
	movl	0x40(%ebx), %eax
	push	%eax
	popfl

	/* Restore the standard registers as far as we can */
	movl	0x00(%ebx), %eax
	movl	0x08(%ebx), %ecx
	movl	0x0c(%ebx), %edx
	movl	0x10(%ebx), %esi
	movl	0x14(%ebx), %edi
	movl	0x18(%ebx), %ebp
	movl	0x1c(%ebx), %esp

	/*
	 * OK, almost there; put cs:eip from the context on the stack so we can
	 * return far into them.
	 */
	pushl	0x24(%ebx)	/* cs */
	pushl	0x20(%ebx)	/* eip */

	/* Finally, restore %ebx and go */
	movl	0x04(%ebx), %ebx
	lret
