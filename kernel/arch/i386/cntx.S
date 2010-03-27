.text

#include <machine/param.h>
#include <machine/vm.h>
#include "asmsyms.h"

.global md_store_ctx, md_restore_ctx, reschedule

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
	movw	$GDT_SEL_KERNEL_PCPU, %bx
	movw	%bx, %fs

	/* Setup ds so we can store the registers */
	movw	$GDT_SEL_KERNEL_DATA, %bx
	movw	%bx, %ds

	/*
	 * We must activate the kernel pagetable because each process can only
	 * see kernel memory and not the thread structures...
	 *
	 * Note that we never update %cr3 in the process-structure because it
	 * cannot be altered by userspace.
	 */
        movl    pagedir, %ebx
        subl    $KERNBASE, %ebx
        movl    %ebx, %cr3

	/* Fetch the address from the context from the per-cpu info */
	movl	%fs:0, %ebx

	/* Store the current registers - we have to do %ebx last */
	movl	%eax, 0x00(%ebx)
	movl	%ecx, 0x08(%ebx)
	movl	%edx, 0x0c(%ebx)
	movl	%esi, 0x10(%ebx)
	movl	%edi, 0x14(%ebx)
	movl	%ebp, 0x18(%ebx)

	/*
	 * We got here by means of a context switch, which means we have to
	 * grab %ss:%esp from the interrupt frame (because we are now using
	 * the kernel ring 0 stack as specified in the TSS)
	 *
	 *      + 32 = interrupted ss
	 *      + 28 = interrupted esp
	 *      + 24 = interrupted eflags
	 *      + 20 = interrupted cs
	 *      + 16 = interrupted eip
	 *      + 12 = (caller md_store_context)
	 *      +  8 = stored ebx
	 *      +  4 = stored fs
	 *  esp +  0 = stored ds
	 */
	movl	28(%esp), %eax
	movl	%eax, 0x1c(%ebx)
	movl	32(%esp), %eax
	movl	%eax, 0x38(%ebx)

	/* Store segment registers es/gs */
	xor	%eax, %eax
	movw	%es,  %ax
	movl	%eax,  0x2c(%ebx)
	movw	%gs,  %ax
	movl	%eax,  0x34(%ebx)

	/* Store original ds */
	popl	%eax
	movl	%eax,  0x28(%ebx)

	/*
	 * Store the FPU context, if needed; %fs still points to the
	 * per-CPU data.
	 */
	movl	%fs:(PCPU_FPUCTX), %eax
	orl	%eax, %eax
	jz	md_store_ctx_skip_fpu

	fsave	(%eax)

	/* Disown the FPU; we have stored it */
	movl	$0, %fs:(PCPU_FPUCTX)

md_store_ctx_skip_fpu:
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
	 * If we are switching to kernel context, this must be because of a
	 * system call that call reschedule(); in this case, we can opt for the
	 * easy way out as we don't have to restore every little detail (the
	 * segment registers, pagetable etc is already correct)
	 *
	 * Note that we have to compare the entire cs:eip thing,
	 * because failure to do so would cause kernel threads to
	 * have problems.
	 */
	cmpl	$GDT_SEL_KERNEL_CODE, 0x24(%ebx)	/* check cs */
	jne	md_restore_ctx2
	cmpl	$reschedule_back, 0x20(%ebx)		/* check eip */
	je	md_restore_ctx_kernel

md_restore_ctx2:

	/*
	 * First of all, restore the segment registers es/fs/gs.
	 */
	movl	0x2c(%ebx), %eax
	movw	%ax, %es
	movl	0x30(%ebx), %eax
	movw	%ax, %fs
	movl	0x34(%ebx), %eax
	movw	%ax, %gs

	/* Restore the standard registers as far as we can */
	movl	0x00(%ebx), %eax
	movl	0x08(%ebx), %ecx
	movl	0x0c(%ebx), %edx
	movl	0x10(%ebx), %esi
	movl	0x14(%ebx), %edi
	movl	0x18(%ebx), %ebp

	/*
	 * We need to switch to the thread's kernelstack as soon as possible,
	 * because we cannot access our kernel stack anymore once we activate
	 * the thread's page directory.
	 */
	movl	0x44(%ebx), %esp

	/*
	 * OK, almost there; note that, on i386, an 'iret' will also pop %ss:%esp
	 * from the stack before fetching the eflags and %cs:%eip; this actually
	 * makes a lot of sense, since you cannot set %ss:%esp beforehand as they
	 * are for different privilege levels...
	 *
	 * The magic phrase is 'If the return is to another privilege level, the
	 * IRET instruction also pops the stack pointer and SS from the stack,
	 * before resuming program execution' (IA-32 Intel Architecture Software
 	 * Developer's Manual Volume 2, page 3-354)
 	 *
	 */
	pushl	0x38(%ebx)	/* ss */
	pushl	0x1c(%ebx)	/* esp */
	pushl	0x40(%ebx)	/* eflags */
	pushl	0x24(%ebx)	/* cs */
	pushl	0x20(%ebx)	/* eip */

	/* Store %eax, we're not quite there yet and need a register we can safely clobber */
	pushl	%eax

	/* Restore ds */
	movl	0x28(%ebx), %eax
	movw	%ax, %ds

	/*
	 * Enable the task-switched flag; this will cause an exception to be
	 * thrown for every FPU instruction. We use this to restore the
	 * adequate context.
	 */
	movl    %cr0, %eax
	orl     $CR0_TS, %eax
	movl    %eax, %cr0

	/*
	 * Grab the page directory in %eax; we can't touch %ebx anymore once we
	 * set it, so place it on the stack.
	 */
	movl	0x3c(%ebx), %eax
	pushl	%eax

	/* Restore ebx and activate paging */
	movl	0x04(%ebx), %ebx

	popl	%eax
	movl	%eax, %cr3

	/* Finally, restore %eax and go */
	popl	%eax
	iret

md_restore_ctx_kernel:
	/*
	 * Restore the saved registers.
	 */
	movl	0x04(%ebx), %ecx		/* ecx = stored ebx! */
	movl	0x10(%ebx), %esi
	movl	0x14(%ebx), %edi
	movl	0x18(%ebx), %ebp

	movl	0x1c(%ebx), %esp

	/*
	 * Note this is a kernel -> kernel return, so we do not have to mess
	 * with a stack. It will be returned by our system call handler.
	 */
	pushl	0x20(%ebx)	/* eip */
	ret

/*
 * Reschedule can be called at any moment and causes two things to happen:
 *
 * - current thread context will be stored
 * - schedule() will be called to schedule a new thread to run
 *
 * Once the function returns, the calling thread will have been re-scheduled,
 * hence the name.
 */
reschedule:
	/*
	 * First of all, we need to store the context. We assume this will
	 * always be called from kernel context - most importantly, this means
	 * both %fs and %cr3 are already set up.
	 *
	 * We do not save/restore any segment registers, as the contex-switch
	 * code is smart enough not to touch them if it's returning to
	 * kernel context.
	 *
	 * System V Application Binary Interface, i386 supplement 4th edition
	 * page 3-11 we only have to store %ebp, %edi, %esi and %ebx.
	 */

	/* Store %ebx/%esi/%ebp/%edi/%esp */
	movl	%fs:0, %eax
	movl	%ebx, 0x04(%eax)
	movl	%esi, 0x10(%eax)
	movl	%edi, 0x14(%eax)
	movl	%ebp, 0x18(%eax)
	movl	%esp, 0x1c(%eax)

	/* Now, insert our return point, %cs:%eip */
	movl	$GDT_SEL_KERNEL_CODE, 0x24(%eax)
	movl	$reschedule_back, 0x20(%eax)
	
	/* All is set - schedule a new thread! */
	call	schedule

	/*
	 * We're back! schedule() calls md_thread_switch()
	 * which will have restored our context (md_restore_ctx()
	 * takes a shortcut as most 'expensive' registers will
	 * be at their correct values already)
	 */

reschedule_back:
	ret
