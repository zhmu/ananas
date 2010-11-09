.text
.globl	entry

#define MSR_IR          0x00000020      /* Interrupt Relocate */
#define MSR_DR          0x00000010      /* Data Relocate */

/*
 * This trampoline code will be placed in the DOL file and is used to setup the
 * Wii so that the kernel can boot without too much problems - we don't need to
 * burden the kernel with all this stuff.
 *
 * Note that, for the Wii, there does not seem to be any consensus on what to
 * expect, so we just re-initialize the BAT tables to the following:
 *
 * [DI]BAT0 - dest   0x00000000-0x07ffffff (32MB)
 *            source 0x00000000
 *            24MB main memory we are executing from (MEM1)
 *    DBAT1 - dest   0xcc000000-0xcdffffff (32MB)
 *            source 0x00c00000
 *            hardware registers
 * [DI]BAT2 - dest   0x10000000-0x13ffffff (64MB)
 *            source 0x10000000
 *            64MB extra memory available for use (MEM2)
 *
 * This will give all available memory to the kernel, and also give most
 * hardware.
 */
entry:
	/*
	 * The 2 lines below are patched to the correct entry point of
	 * the kernel as stored in the ELF file.
	 */
	lis	%r30, 0x1234		/* patched to hi part of address */
	ori	%r30, %r30, 0x4321	/* patched to lo part of address */

	/*
	 * First of all, make sure we run from an identity-mapped location;
	 * this ensures we can safely alter the BAT registers without
	 * crashing in between.
	 */
	lis	%r3, start@ha
	addi	%r3, %r3, start@l
	mfmsr	%r4
	rlwinm	%r5, %r4, 0, 26, 27	/* obtain DR and IR bits in r5... */
	subf	%r4, %r5, %r4		/* ...and clear them in r4 */
	mtsrr0	%r3
	mtsrr1	%r4
	rfi

start:
	/*
	 * Once we are here, we're running from identity-mapped addresses and
	 * translation is disabled. Need to set up the BAT registers now.
	 */

	/* DIBAT0 */
	li	%r7, 0x03ff		/* to: 0x0, size: 32MB (can't map 24MB) */
	li	%r8, 0x2		/* from 0x0, WIMG=0000, PP=10 */
	mtibatu	0, %r7
	mtdbatu	0, %r7
	mtibatl	0, %r8
	mtdbatl	0, %r8

	/* DBAT1 */
	lis	%r7, 0xcc00
	ori	%r7, %r7, 0x03ff	/* to:  0xcc00000, size: 32MB (up to 0xcdffffff) */
	lis	%r8, 0x0c00	
	ori	%r8, %r8, 0x002a	/* from 0x0c00000, WIMG=0101, PP=11 */
	mtdbatu	1, %r7
	mtdbatl	1, %r8

	/* DIBAT2 */
	lis	%r7, 0x1000
	ori	%r7, %r7, 0x07ff	/* to: 0x10000000, size: 64MB */
	lis	%r8, 0x1000
	ori	%r8, %r8, 0x2		/* from: 0x10000000, WIMG=0000, PP=10 */
	mtibatu	2, %r7
	mtdbatu	2, %r7
	mtibatl	2, %r8
	mtdbatl 2, %r8
	sync
	isync

	/* Switch on the DR/IR bits */
	mfmsr	%r4
	ori	%r4, %r4, (MSR_DR | MSR_IR)
	mtmsr	%r4
	isync

	/*
	 * We're still breathing; have the DVD drive light up blue so that it's
	 * visible that the trampoline code worked fine.
	 */
	lis	%r3, 0xcd00
	ori	%r3, %r3, 0xc0
	lwz	%r2, 0(%r3)
	ori	%r2, %r2, 0x20
	stw	%r2, 0(%r3)

	/* Jump to the ELF entry point */
	mtsrr0	%r30
	mtsrr1	%r4
	rfi
