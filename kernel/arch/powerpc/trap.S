.text
.globl	generic_int, generic_int_len
.globl	syscall_int, syscall_int_len
.globl	dsi_trap, dsi_trap_len
.globl	md_thread_switch
.globl	md_reschedule

.globl	trapreturn /* DEBUG */

#include "asmsyms.h"
#include <machine/vm.h>

/*
 * This code will be copied to most interrupt vectors; it determines
 * where it came from and then chains through to the real exception
 * code.
 *
 * Will be called with: %srr0 = instruction causing the trap
 *                      %srr1 = updated msr
 */
generic_int:
	mtsprg1	%r1		/* Store old SP */
	mflr	%r1		/* Store old LR */
	bla	trapcode

generic_int_len = . - generic_int

/*
 * XXX This chain is only to ease debugging, so the actual system call
 * code corresponds to entries in our symbols)
 */
syscall_int:
	ba	syscallcode

syscall_int_len = . - syscall_int

/* Stores all registers; assumes %r1 points to the stackframe to use */
#define STORE_REGS \
	stw	%r0 ,  SF_R0(%r1);		\
	/* stw	%r1 ,  SF_R1(%r1); */		\
	stw	%r2 ,  SF_R2(%r1);		\
	stw	%r3 ,  SF_R3(%r1);		\
	stw	%r4 ,  SF_R4(%r1);		\
	stw	%r5 ,  SF_R5(%r1);		\
	stw	%r6 ,  SF_R6(%r1);		\
	stw	%r7 ,  SF_R7(%r1);		\
	stw	%r8 ,  SF_R8(%r1);		\
	stw	%r9 ,  SF_R9(%r1);		\
	stw	%r10, SF_R10(%r1);		\
	stw	%r11, SF_R11(%r1);		\
	stw	%r12, SF_R12(%r1);		\
	stw	%r13, SF_R13(%r1);		\
	stw	%r14, SF_R14(%r1);		\
	stw	%r15, SF_R15(%r1);		\
	stw	%r16, SF_R16(%r1);		\
	stw	%r17, SF_R17(%r1);		\
	stw	%r18, SF_R18(%r1);		\
	stw	%r19, SF_R19(%r1);		\
	stw	%r20, SF_R20(%r1);		\
	stw	%r21, SF_R21(%r1);		\
	stw	%r22, SF_R22(%r1);		\
	stw	%r23, SF_R23(%r1);		\
	stw	%r24, SF_R24(%r1);		\
	stw	%r25, SF_R25(%r1);		\
	stw	%r26, SF_R26(%r1);		\
	stw	%r27, SF_R27(%r1);		\
	stw	%r28, SF_R28(%r1);		\
	stw	%r29, SF_R29(%r1);		\
	stw	%r30, SF_R30(%r1);		\
	stw	%r31, SF_R31(%r1);		\
	mfxer	%r3;				\
	mfctr	%r4;				\
	mfcr	%r5;				\
	stw	%r3,  SF_XER(%r1);		\
	stw	%r4,  SF_CTR(%r1);		\
	stw	%r5,   SF_CR(%r1);		\
	mfsrr0	%r3;				\
	mfsrr1	%r4;				\
	stw	%r3,  SF_SRR0(%r1);		\
	stw	%r4,  SF_SRR1(%r1);		\
	mfsprg2	%r3;				\
	stw	%r3,   SF_LR(%r1);		\
	mfsprg1	%r3;				\
	stw	%r3,   SF_R1(%r1);

/* Restores all registers; assumes %r1 points to the stackframe to use */
#define RESTORE_REGS \
	lwz	%r3,  SF_XER(%r1);		\
	lwz	%r4,  SF_CTR(%r1);		\
	lwz	%r5,   SF_LR(%r1);		\
	lwz	%r6,   SF_CR(%r1);		\
	mtxer	%r3;				\
	mtctr	%r4;				\
	mtlr	%r5;				\
	mtcr	%r6;				\
	lwz	%r3, SF_SRR0(%r1);		\
	lwz	%r4, SF_SRR1(%r1);		\
	mtsrr0	%r3;				\
	mtsrr1	%r4;				\
	lwz	%r0 ,  SF_R1(%r1);		\
	mtsprg2	%r0;				\
	lwz	%r0 ,  SF_R0(%r1);		\
	lwz	%r2 ,  SF_R2(%r1);		\
	lwz	%r3 ,  SF_R3(%r1);		\
	lwz	%r4 ,  SF_R4(%r1);		\
	lwz	%r5 ,  SF_R5(%r1);		\
	lwz	%r6 ,  SF_R6(%r1);		\
	lwz	%r7 ,  SF_R7(%r1);		\
	lwz	%r8 ,  SF_R8(%r1);		\
	lwz	%r9 ,  SF_R9(%r1);		\
	lwz	%r10, SF_R10(%r1);		\
	lwz	%r11, SF_R11(%r1);		\
	lwz	%r12, SF_R12(%r1);		\
	lwz	%r13, SF_R13(%r1);		\
	lwz	%r14, SF_R14(%r1);		\
	lwz	%r15, SF_R15(%r1);		\
	lwz	%r16, SF_R16(%r1);		\
	lwz	%r17, SF_R17(%r1);		\
	lwz	%r18, SF_R18(%r1);		\
	lwz	%r19, SF_R19(%r1);		\
	lwz	%r20, SF_R20(%r1);		\
	lwz	%r21, SF_R21(%r1);		\
	lwz	%r22, SF_R22(%r1);		\
	lwz	%r23, SF_R23(%r1);		\
	lwz	%r24, SF_R24(%r1);		\
	lwz	%r25, SF_R25(%r1);		\
	lwz	%r26, SF_R26(%r1);		\
	lwz	%r27, SF_R27(%r1);		\
	lwz	%r28, SF_R28(%r1);		\
	lwz	%r29, SF_R29(%r1);		\
	lwz	%r30, SF_R30(%r1);		\
	lwz	%r31, SF_R31(%r1);		\
	mfsprg2	%r1

/*
 * Restores all segment registers; assumes %r1 points to the stackframe to use
 * note that it also assumes that %sr0 must be restored last!
 */
#define RESTORE_SREGS(s) \
	lwz	%r30 , SF_SR15(s); mtsr 15, %r30; \
	lwz	%r30 , SF_SR14(s); mtsr 14, %r30; \
	lwz	%r30 , SF_SR13(s); mtsr 13, %r30; \
	lwz	%r30 , SF_SR12(s); mtsr 12, %r30; \
	lwz	%r30 , SF_SR11(s); mtsr 11, %r30; \
	lwz	%r30 , SF_SR10(s); mtsr 10, %r30; \
	lwz	%r30 ,  SF_SR9(s); mtsr  9, %r30; \
	lwz	%r30 ,  SF_SR8(s); mtsr  8, %r30; \
	lwz	%r30 ,  SF_SR7(s); mtsr  7, %r30; \
	lwz	%r30 ,  SF_SR6(s); mtsr  6, %r30; \
	lwz	%r30 ,  SF_SR5(s); mtsr  5, %r30; \
	lwz	%r30 ,  SF_SR4(s); mtsr  4, %r30; \
	lwz	%r30 ,  SF_SR3(s); mtsr  3, %r30; \
	lwz	%r30 ,  SF_SR2(s); mtsr  2, %r30; \
	lwz	%r30 ,  SF_SR1(s); mtsr  1, %r30; \
	lwz	%r30 ,  SF_SR0(s); mtsr  0, %r30; \
	isync

/*
 * This is the generic trapcode; it will be called with:
 *
 * - %r1     saved original %lr
 * - %sprg1  saved original %r1
 *
 * The reason %lr is saved is because we use a linked branch to get to this
 * code, which overwrites %lr - the new %lr is used to figure out where we
 * came from, and thus, which exception this was in the first place.
 *
 * Note that this will be run with paging disabled.
 */
trapcode:
	/*
	 * First of all, we must make a trapframe to store our context into.
	 * This should be restored upon handler completion.
	 */
	mtsprg2	%r1				/* Store saved LR */
	mfsprg0	%r1				/* Get PCPU data */
	lwz	%r1,(PCPU_CONTEXT)(%r1)		/* Get current context in r1 */

trapshortcut:
	/* Store all registers we can */
	STORE_REGS

	/* Determine exception number */
	mflr	%r3
	andi.	%r3, %r3, 0xff00
	stw	%r3,  SF_EXC(%r1)

	/* Set up our argument */
	mr	%r3,%r1

	/* Restore the kernel segment registers */
	lis	%r1, bsp_sf@ha
	addi	%r1, %r1, bsp_sf@l
	RESTORE_SREGS(%r1)

	/*
	 * Enable instruction/data translation. Note that we have to use the
	 * current %msr (not the one from %srr0) as we don't want to switch
	 * things like the PR bit.
	 */
	mfmsr	%r2
	ori	%r2, %r2, (MSR_DR | MSR_IR)
	mtmsr	%r2
	sync

	/* Activate the interupt kernel stack; it's available and we can be sure
	 * it's mapped (XXX This will mess up in SMP - we need per-cpu stacks
	 * there!)
	 */
	lis	%r1, int_stack@ha
	addi	%r1, %r1, int_stack@l

	/* Launch the exception handler! */
	bl	exception_handler

trapreturn:

	/* Restore our registers */
	mfsprg0	%r1				/* Get PCPU data */
	lwz	%r1,(PCPU_CONTEXT)(%r1)		/* Get current context in r1 */

	RESTORE_REGS

	/*
	 * Restored %r1 still lives in %sprg2; so we don't have to save it.
	 * Do store %r30 though - %r1 and %sprg2 are used by RESTORE_SREGS
	 */
	mtsprg3 %r30

	/* Restore %r1 context so we can restore segment regs */
	mfsprg0	%r1
	lwz	%r1,(PCPU_CONTEXT)(%r1)		/* Get current context in r1 */
	RESTORE_SREGS(%r1)

	/* Finally, restore saved %r1 and %r30 */
	mfsprg2	%r1
	mfsprg3	%r30

	rfi

/*
 * This is our data storage interrupt; it will be called whenever an
 * instruction attempts to access data that is neither mapped by a BAT
 * entry, nor by the pagetables.
 *
 * If the trap came from kernel mode, we attempt to activate a BAT mapping
 * if necessary. This is done to prevent a lot of pagetable entries wasted
 * to be able to talk to devices (and the OpenFirmware will use them as well)
 */
dsi_trap:
	ba	dsicode
dsi_trap_len = . - dsi_trap
	
dsicode:
	mtsprg1	%r1				/* Store old SP */
	mfsprg0	%r1				/* Get PCPU data */
	lwz	%r1,(PCPU_CONTEXT)(%r1)		/* Get current context in r1 */
	stw	%r2,SF_R2(%r1)			/* Free %r2 */
	stw	%r3,SF_R3(%r1)			/* Free %r3 */
	mfcr	%r2
	stw	%r2,SF_CR(%r1)			/* Store %cr */

	/*
	 * As claimed above, we need to figure out whether we are in kernel mode.
	 * This can easily be done by transferring the saved Machine Status
	 * Word to the condition register as we can then immediately check the
	 * desired PR bit (17).
	 */
	mfsrr1	%r2
	mtcr	%r2
	bt	17, dsiuser

	/* We are in kernel mode; now let's find out which BAT entry to use */
	mfdar	%r2
	rlwinm	%r2,%r2,7,25,28			/* %r2 = (addr rol 7) & 0x78 = hi 4 bits * 8 */

	addis	%r2,%r2,bat@ha			/* %r2 = bat (hi 16 bits) | %r2 */
	lwz	%r3,bat+0@l(%r2)		/* %r3 = bat[fault addr].bat_u */
	lwz	%r2,bat+4@l(%r2)		/* %r2 = bat[fault addr].bat_l */

	mtcr	%r3				/* if bat_u, bit 30 (Vs) is clear... */
	bf	30, dsiuser			/* ... entry will not be valid, bail */

	mtdbatu	2, %r3
	mtdbatl	2, %r2

	/*
	 * BAT updated; now, we have to restore our registers and return to the
	 * trapped code.
 	*/
	lwz	%r2,  SF_CR(%r1)
	mtcr	%r2				/* Restore %cr */

	lwz	%r2, SF_R2(%r1)			/* Restore %r2 */
	lwz	%r3, SF_R3(%r1)			/* Restore %r3 */
	
	mfsprg1	%r1				/* Restore %r1 */
	rfi

dsiuser:
	/*
	 * Either no BAT entry was found, or we were in user mode; restore the
	 * registers we messed up and take a shortcut to the trapcode. We do this
	 * by faking a call from 0x300, which is the DSI code.
	 */

	mflr	%r2
	mtsprg2	%r2				/* Store LR */

	li	%r2, 0x300
	mtlr	%r2				/* Fake 0x300 in LR */

	mfdar	%r2
	stw	%r2, SF_DAR(%r1)		/* Store %dar */

	mfdsisr	%r2
	stw	%r2, SF_DSISR(%r1)		/* Store %disr */
	
	lwz	%r2, SF_CR(%r1)
	mtcr	%r2				/* Restore %cr */

	lwz	%r2, SF_R2(%r1)			/* Restore %r2 */
	lwz	%r3, SF_R3(%r1)			/* Restore %r3 */

	/*
	 * Note that trapshortcut expects %r1 = percpu context and
	 * %sprg1 is saved %r1.
	 */
	ba	trapshortcut

md_thread_switch:
	/*
	 * Per ELF PowerPC ABI, %r3 = new thread, %r4 = old thread
	 */
	addi	%r1,%r3,THREAD_SF 		/* Get thread context in %r1 */

	/* Copy the thread's %r1 to %sprg2 - this is what RESTORE_REGS will restore */
	lwz	%r2, SF_R1(%r1)
	mtsprg2	%r2

	/* Update our current context in the per-CPU structure */
	mfsprg0	%r2
	stw	%r1,(PCPU_CONTEXT)(%r2)
	stw	%r3,(PCPU_CURTHREAD)(%r2)

	/*
	 * Once we restore our segment registers, we cannot touch our thread
	 * structure anymore as it will be inaccessible. Thus, we will have to
	 * restore the registers as much as we can, and then touch the
	 * segment registers.
	 */ 
	mtsprg1	%r1		/* Save %r1 in %sprg1 */

	RESTORE_REGS

	/* Store restored %r1 and %r30 - these are used by RESTORE_SREGS */
	mtsprg2	%r1
	mtsprg3 %r30

	/* Restore %r1 context so we can restore segment regs */
	mfsprg1	%r1
	RESTORE_SREGS(%r1)

	/* Finally, restore saved %r1 and %r30 */
	mfsprg2	%r1
	mfsprg3	%r30
	rfi

syscallcode:
	/*
	 * According to the E500 architecture ABI, we do not need to store
	 * all registers; only r1, r2, r13-r31, lr and cr must be saved -
	 * but we also need srr0 and srr1 as these determine the context
	 * we will return to.
	 *
	 * What we do is, we store these on the caller's stack; this makes
	 * things work should we get interrupted by an interrupt, since it
	 * will just restore us to where we were. We make a standard stack-
	 * frame, which eases debugging and already stores %lr and %r31,
	 * which we need to store anyway.
	 * 
	 * XXX is storing r13 really needed?
	 *
	 * XXX we shouldn't use the userland stack for storing temp data,
	 *     it can be influenced by other threads and is insecure XXX
	 */

	/*
	 * First of all, enable data/instruction translation; this gives us access
	 * to the caller's stack.
	 */
	mfmsr	%r10
	ori	%r10, %r10, (MSR_DR | MSR_IR)
	mtmsr	%r10
	isync				/* XXX is this necessary? mtmsr is context switching? */

	/* Create the stack frame */
	stwu	%r1, -128(%r1)
	mflr	%r10			/* store LR */
	stw	%r10, 132(%r1)
	stw	%r31, 124(%r1)		/* store %r31 */
	mr	%r31, %r1		/* %r31 = stack frame */

	/* Store the registers outlined above */
	stw	%r2,   16(%r31)
	stw	%r13,  20(%r31)
	stw	%r14,  24(%r31)
	stw	%r15,  28(%r31)
	stw	%r16,  32(%r31)
	stw	%r17,  36(%r31)
	stw	%r18,  40(%r31)
	stw	%r19,  44(%r31)
	stw	%r20,  48(%r31)
	stw	%r21,  52(%r31)
	stw	%r22,  56(%r31)
	stw	%r23,  60(%r31)
	stw	%r24,  64(%r31)
	stw	%r25,  68(%r31)
	stw	%r26,  72(%r31)
	stw	%r27,  76(%r31)
	stw	%r28,  80(%r31)
	stw	%r29,  84(%r31)
	stw	%r30,  88(%r31)
	mfcr	%r10
	mfsrr0	%r11
	mfsrr1	%r12
	stw	%r10,  92(%r31)
	stw	%r11,  96(%r31)
	stw	%r12, 100(%r31)

	/*
	 * OK; we have stored enough of our context. We can use any register
	 * we want - so let's start by responding the kernel's segments. We
	 * need this in order to access the thread's kernel stack, as the
	 * thread itself will not have it mapped.
	 */
	lis	%r10, bsp_sf@ha
	addi	%r10, %r10, bsp_sf@l
	RESTORE_SREGS(%r10)

	/* Let us switch to our process kernel stack */
	mfsprg0	%r10				/* Get PCPU data */
	lwz	%r10,(PCPU_CURTHREAD)(%r10)
	lwz	%r1,(THREAD_KSTACK)(%r10)

	/* Create the SYSCALL_ARGS structure */
	stwu	%r1,-64(%r1)		/* XXX should be x >= sizeof(args) where x mod 16 = 0 */
	addi	%r30, %r1, 8
	stw	%r0, SC_NUMBER(%r30)
	stw	%r3, SC_ARG1(%r30)
	stw	%r4, SC_ARG2(%r30)
	stw	%r5, SC_ARG3(%r30)
	stw	%r6, SC_ARG4(%r30)
	stw	%r7, SC_ARG5(%r30)

	/*
	 * Launch the system call handler - we use the fact that
	 * it wil not touch %r31 here
	 */
	mr	%r3, %r30
	bl	syscall

	/*
	 * Restore the thread segment registers; we cannot access the user stack
	 * without doing so.
	 */
	mfsprg0	%r1
	lwz	%r1,(PCPU_CONTEXT)(%r1)
	RESTORE_SREGS(%r1)

	/* Restore the registers */
	lwz	%r10, 100(%r31)
	lwz	%r11,  96(%r31)
	lwz	%r12,  92(%r31)
	mtsrr0	%r11
	mtsrr1	%r10
	mtcr	%r12
	lwz	%r30,  88(%r31)
	lwz	%r29,  84(%r31)
	lwz	%r28,  80(%r31)
	lwz	%r27,  76(%r31)
	lwz	%r26,  72(%r31)
	lwz	%r25,  68(%r31)
	lwz	%r24,  64(%r31)
	lwz	%r23,  60(%r31)
	lwz	%r22,  56(%r31)
	lwz	%r21,  52(%r31)
	lwz	%r20,  48(%r31)
	lwz	%r19,  44(%r31)
	lwz	%r18,  40(%r31)
	lwz	%r17,  36(%r31)
	lwz	%r16,  32(%r31)
	lwz	%r15,  28(%r31)
	lwz	%r14,  24(%r31)
	lwz	%r13,  20(%r31)
	lwz	%r2,   16(%r31)

	/* Tear down our stack frame */
	addi	%r11, %r31, 128
	lwz	%r0,  4(%r11)
	mtlr	%r0
	lwz	%r31, -4(%r11)
	mr	%r1,  %r11
	rfi

md_reschedule:
	/*
	 * reschedule will be called from kernel context; all we need to
	 * do is to update the caller's context so the next scheduling
	 * takes us to reschedule_back - in effect, we are scheduling
	 * a kernel-ish thread.
	 *
	 * Note that we only need to store part of our context, as this
	 * will be considered a function call (the compiler doesn't care
	 * that we have done the whole scheduler dance!)
	 */
	mfsprg0	%r10				/* Get PCPU data */
	lwz	%r10,(PCPU_CONTEXT)(%r10)	/* Get current context in r10 */

	/* Store the registers as required by the ABI */
	stw	%r1 ,  SF_R1 (%r10)
	stw	%r2 ,  SF_R2 (%r10)
	stw	%r13,  SF_R13(%r10)
	stw	%r14,  SF_R14(%r10)
	stw	%r15,  SF_R15(%r10)
	stw	%r16,  SF_R16(%r10)
	stw	%r17,  SF_R17(%r10)
	stw	%r18,  SF_R18(%r10)
	stw	%r19,  SF_R19(%r10)
	stw	%r20,  SF_R20(%r10)
	stw	%r21,  SF_R21(%r10)
	stw	%r22,  SF_R22(%r10)
	stw	%r23,  SF_R23(%r10)
	stw	%r24,  SF_R24(%r10)
	stw	%r25,  SF_R25(%r10)
	stw	%r26,  SF_R26(%r10)
	stw	%r27,  SF_R27(%r10)
	stw	%r28,  SF_R28(%r10)
	stw	%r29,  SF_R29(%r10)
	stw	%r30,  SF_R30(%r10)
	stw	%r31,  SF_R31(%r10)
	mflr	%r14
	mfcr	%r15
	lis	%r16,  reschedule_back@ha
	addi	%r16,  %r16, reschedule_back@l
	mfsrr1	%r17
	li	%r17, 0x30 /* XXX fix me; we're boosting the thread here! */
	stw	%r14,  SF_LR(%r10)
	stw	%r15,  SF_CR(%r10)
	stw	%r16, SF_SRR0(%r10)
	stw	%r17, SF_SRR1(%r10)

	/* Force the scheduler to pick a new thread */
	b	schedule

reschedule_back:
	/*
	 * XXX force the kernel segmentation regs; we should
	 *     add a flag to preventing the user-segment
	 *     restoration as we are effectively undoing it.
	 */
	lis	%r10, bsp_sf@ha
	addi	%r10, %r10, bsp_sf@l
	RESTORE_SREGS(%r10)

	/* XXX force the thread back to user mode */

	blr

.data
.align  4
        .space  2048
int_stack:

.globl  int_stack
