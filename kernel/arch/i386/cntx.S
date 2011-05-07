.text

#include <machine/param.h>
#include <machine/vm.h>
#include "asmsyms.h"

.global md_store_ctx, md_restore_ctx, md_reschedule, md_idle_thread

#define SANITY_CHECKS

/*
 * Stores the current thread context. This function expects to be
 * called from an interrupt context, as a near call.
 */
md_store_ctx:
	/*
	 * Disable any interrupts; we do not want to context to change while
	 * we are storing it.
	 */
	pushfl
	cli

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

	/* Fetch the address from the context from the per-cpu info */
	movl	%fs:(PCPU_CONTEXT), %ebx

	/* Store the current registers - we have to do %ebx last */
	movl	%eax, CTX_EAX(%ebx)
	movl	%ecx, CTX_ECX(%ebx)
	movl	%edx, CTX_EDX(%ebx)
	movl	%esi, CTX_ESI(%ebx)
	movl	%edi, CTX_EDI(%ebx)
	movl	%ebp, CTX_EBP(%ebx)

	/*
	 * The i386 CPU does not store the original %ss:%esp if the interrupt
	 * occured in the same privilege level.
	 *
	 * Unfortunately, we'll have to detect this condition ourselves and
	 * store our original %ss:%esp (we only do the latter because %ss is
	 * fixed anyway)
	 */
	cmpl	$GDT_SEL_KERNEL_CODE, 24(%esp)
	je	md_store_kernel

	/*
	 * We got here by means of a context switch, which means we have to
	 * grab %ss:%esp from the interrupt frame (because we are now using
	 * the kernel ring 0 stack as specified in the TSS)
	 *
	 *      + 36 = interrupted ss  (iff ring3->0 transition)
	 *      + 32 = interrupted esp (iff ring3->0 transition)
	 *      + 28 = interrupted eflags
	 *      + 24 = interrupted cs
	 *      + 20 = interrupted eip
	 *      + 16 = (caller md_store_context)
	 *      + 12 = stored eflags
	 *      +  8 = stored ebx
	 *      +  4 = stored fs
	 *  esp +  0 = stored ds
	 */
	movl	32(%esp), %eax
	movl	%eax, CTX_ESP(%ebx)
	movl	36(%esp), %eax
	movl	%eax, CTX_SS(%ebx)

	/* Store segment registers es/gs */
	xor	%eax, %eax
	movw	%es,  %ax
	movl	%eax, CTX_ES(%ebx)
	movw	%gs,  %ax
	movl	%eax, CTX_GS(%ebx)

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
	/* Store original ds */
	popl	%eax
	movl	%eax, CTX_DS(%ebx)

	/* Store original fs */
	popl	%eax
	movl	%eax, CTX_FS(%ebx)

	/* Store ebx */
	movl	%ebx, %edx
	popl	%ebx
	movl	%ebx, CTX_EBX(%edx)

	/*
	 * Store cs:eip and eflags - we grab those from the interrupt context.
	 * Note that these are 12 earlier (i.e. eip is at esp+4 instead of
	 * esp+16) because we have pop'ed %ebx/fs/ds a few lines back.
	 */
	movl	8(%esp), %eax		/* eip */
	movl	%eax, CTX_EIP(%edx)
	movl	12(%esp), %eax		/* cs */
	movl	%eax, CTX_CS(%edx)
	movl	16(%esp), %eax		/* eflags */
	movl	%eax, CTX_EFLAGS(%edx)

	/* Restore original flags */
	popfl
	ret

md_store_kernel:
	/*
	 * Store our original %esp; this is before the
	 * entire interrupt makeover. We'll have
	 * 4x push in this function, a call return address,
	 * the stored cs:eip and eflags, total 7 dwords.
	 */
	movl	%esp, %eax
	addl	$(8 * 4), %eax
	movl	%eax, CTX_ESP(%ebx)

	/* Skip segment registers and FPU; they'll never be used */
	jmp	md_store_ctx_skip_fpu

/*
 * Restores the thread context and jumps to the thread
 *
 * 4(%ebp) = ctx
 */
md_restore_ctx:
	movl	4(%esp), %ebx

	/*
	 * First of all, restore thread's pagedir; we skip this if possible (it
	 * forces a TLB flush which is expensive)
	 */
	movl	CTX_CR3(%ebx), %eax
	movl	%cr3, %ecx
	cmpl	%eax, %ecx
	je	md_restore_ctx_skip_cr3
	movl	%eax, %cr3
md_restore_ctx_skip_cr3:

	/*
	 * First of all, restore the segment registers es/fs/gs.
	 */
	movl	CTX_ES(%ebx), %eax
	movw	%ax, %es
	movl	CTX_FS(%ebx), %eax
	movw	%ax, %fs
	movl	CTX_GS(%ebx), %eax
	movw	%ax, %gs

	/* Restore the standard registers as far as we can */
	movl	CTX_EAX(%ebx), %eax
	movl	CTX_ECX(%ebx), %ecx
	movl	CTX_EDX(%ebx), %edx
	movl	CTX_ESI(%ebx), %esi
	movl	CTX_EDI(%ebx), %edi
	movl	CTX_EBP(%ebx), %ebp

	/*
	 * OK, almost there; note that, on i386, an 'iret' will also pop %ss:%esp
	 * from the stack before fetching the eflags and %cs:%eip; this actually
	 * makes a lot of sense, since you cannot set %ss:%esp beforehand as they
	 * are for different privilege levels... (thus, should we be transitioning
	 * to a kernel thread, we musn't do this!)
	 *
	 * The magic phrase is 'If the return is to another privilege level, the
	 * IRET instruction also pops the stack pointer and SS from the stack,
	 * before resuming program execution' (IA-32 Intel Architecture Software
	 * Developer's Manual Volume 2, page 3-354)
	 */
	cmpl	$GDT_SEL_KERNEL_CODE, CTX_CS(%ebx)
	jne	r_uthread

	/*
	 * Need to restore a kernel thread; note that %ss:%esp will not be
	 * restored because of the reasoning above, so we'll have to do
	 * work around this.
	 *
	 * We do not need to restore %cr3 or %ds as these are already the
	 * kernel versions. All we have to do is enter the thread with
	 * our correct stackpointer.
	 *
	 * XXX We don't clear CR0_TS (but kernel code shouldn't use FP anyway)
	 */
	movl	CTX_ESP(%ebx), %esp

	pushl	CTX_EFLAGS(%ebx)	/* eflags */
	pushl	CTX_CS(%ebx)		/* cs */
	pushl	CTX_EIP(%ebx)		/* eip */

	/* Restore %ds */
	pushl	CTX_DS(%ebx)
	popl	%ds

	/* Finally, restore ebx itself and go */
	movl	CTX_EBX(%ebx), %ebx
	iret

r_uthread:
	/* Prepare the stack for return */
	pushl	CTX_SS(%ebx)		/* userland ss */
	pushl	CTX_ESP(%ebx)		/* userland esp */
	pushl	CTX_EFLAGS(%ebx)	/* eflags */
	pushl	CTX_CS(%ebx)		/* cs */
	pushl	CTX_EIP(%ebx)		/* eip */

	/* Store %eax, we're not quite there yet and need a register we can safely clobber */
	pushl	%eax

	/* Restore ds */
	movl	CTX_DS(%ebx), %eax
	movw	%ax, %ds

#ifdef SANITY_CHECKS
	/*
	 * Santity check: we must restore the userland ds; if we don't, the
	 * iret will set %ds to the null descriptor - causing hard-to-debug
	 * problems. Best to catch it beforehand.
	 */
	cmpw	$(GDT_SEL_USER_DATA + 3), %ax
	je	md_restore_ctx_dsok

	int	$3

md_restore_ctx_dsok:
#endif

	/*
	 * Enable the task-switched flag; this will cause an exception to be
	 * thrown for every FPU instruction. We use this to restore the
	 * adequate context.
	 */
	movl    %cr0, %eax
	orl     $CR0_TS, %eax
	movl    %eax, %cr0

	/* Restore %ebx and %eax */
	movl	CTX_EBX(%ebx), %ebx
	popl	%eax
	iret

/*
 * Reschedule can be called at any moment and causes two things to happen:
 *
 * - current thread context will be stored
 * - schedule() will be called to schedule a new thread to run
 *
 * Once the function returns, the calling thread will have been re-scheduled,
 * hence the name.
 */
md_reschedule:
	/*
	 * First of all, we need to store the context. We assume this will
	 * always be called from kernel context - most importantly, this means
	 * %fs is already set up to the appropriate per-cpu context.
	 *
	 * We do not save/restore any segment registers, as the contex-switch
	 * code is smart enough not to touch them if it's returning to
	 * kernel context.
	 *
	 * System V Application Binary Interface, i386 supplement 4th edition
	 * page 3-11 we only have to store %ebp, %edi, %esi and %ebx.
	 *
	 * If we are in kernel context, thus %fs:(PCPU_CONTEXT) is zero, we
	 * shortcut and just wait for the very first interrupt that shows up.
	 */
	movl	%fs:(PCPU_CONTEXT), %eax
	orl	%eax, %eax
	jz	reschedule_kernctx

	/*
	 * Temporarily override %fs with our PCPU data; this is necessary
	 * because we will be returned to kernel context. After rescheduling
	 * is done, we'll restore the previous value - this prevents a
	 * bogus %fs restore during context restoring of this thread, which
	 * is expensive.
	 *
	 * Note that we must do this before we store %esp so that we restore
	 * the correct %esp when we are re-scheduled.
	 */
	pushl	CTX_FS(%eax)
	movl	$GDT_SEL_KERNEL_PCPU, CTX_FS(%eax)

	/* Store %ebx/%esi/%ebp/%edi/%esp */
	movl	%ebx, CTX_EBX(%eax)
	movl	%esi, CTX_ESI(%eax)
	movl	%edi, CTX_EDI(%eax)
	movl	%ebp, CTX_EBP(%eax)
	movl	%esp, CTX_ESP(%eax)

	/* Now, insert our return point, %cs:%eip */
	movl	$GDT_SEL_KERNEL_CODE, CTX_CS(%eax)
	movl	$reschedule_back, CTX_EIP(%eax)
	
	/* All is set - schedule a new thread! */
	jmp	schedule

reschedule_back:
	/*
	 * We're back! schedule() calls md_thread_switch()
	 * which will have restored our context (md_restore_ctx()
	 * takes a shortcut as most 'expensive' registers will
	 * be at their correct values already). Note that we'll
	 * have to update the thread's %fs.
	 */
	movl	%fs:(PCPU_CONTEXT), %eax
	popl	CTX_FS(%eax)
	ret
	
reschedule_kernctx:
	hlt
	ret

/*
 * This is our idle thread; it just waits for an interrupt.
 */
md_idle_thread:
	hlt
	jmp	md_idle_thread
