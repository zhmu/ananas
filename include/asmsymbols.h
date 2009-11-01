#ifndef __ASM_SYMS_H__
#define __ASM_SYMS_H__

/*
 * This macro defines each symbol S to sym_S_N, for N = 0..3. The idea is that
 * these symbols end up in a seperate include file, and we can use this file
 * to generate an include file.
 *
 * Why the hassle? The problem is that we can't feed struct's and other more
 * complicated declarations into the assembler, which is a pain if we want to
 * use struct fields in a .S file.
 *
 * The modus operandi is heavily influenced by FreeBSD's assyms structure.
 */
#define ASM_SYMBOL(x, y) \
	unsigned char sym_ ## x ## _0[((uint64_t)(y)      ) & 0xffff]; \
	unsigned char sym_ ## x ## _1[((uint64_t)(y) >> 16) & 0xffff]; \
	unsigned char sym_ ## x ## _2[((uint64_t)(y) >> 32) & 0xffff]; \
	unsigned char sym_ ## x ## _3[((uint64_t)(y) >> 48) & 0xffff];

#define offsetof __builtin_offsetof /* XXX should be in a cdefs.h file */

#endif /* __ASM_SYMS_H__ */
