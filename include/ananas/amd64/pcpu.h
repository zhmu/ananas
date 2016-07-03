#ifndef __AMD64_PCPU_H__
#define __AMD64_PCPU_H__

#include <ananas/cdefs.h>

/* amd64-specific per-cpu structure */
#define MD_PCPU_FIELDS								\
	void		*context;						\
	/* syscall_rsp and rsp0 are used to save/restore the userland's %rsp	\
	 * during a SYSCALL instruction - %rsp0 will be the kernel %rsp to set	\
	 * for the syscall.							\
	 */									\
	addr_t		syscall_rsp;						\
	addr_t		rsp0;							\
	addr_t		tss;							\
	/*									\
	 * fpu_context is used to refer to the struct that holds the current	\
	 * FPU context, or NULL if there is none. Being non-NULL means the	\
	 * current thread is using the FPU and thus the context must be saved.	\
	 */									\
	void		*fpu_context;						\
	/* Per-cpu interrupt tick counter */					\
	uint32_t	tickcount;

#define PCPU_TYPE(x) \
	__typeof(((struct PCPU*)0)->x)

#define PCPU_OFFSET(x) \
	__builtin_offsetof(struct PCPU, x)

#define PCPU_GET(name) ({							\
	PCPU_TYPE(name) p;							\
	static_assert(sizeof(p) == 1 || sizeof(p) == 2 || sizeof(p) == 4 || sizeof(p) == 8, "unsupported field size"); \
	__asm __volatile (							\
		"mov %%gs:%1, %0"						\
		: "=r" (p)							\
		: "m" (*(uint32_t*)(PCPU_OFFSET(name)))				\
	);									\
	p;									\
})

#define PCPU_SET(name, val) do {						\
	PCPU_TYPE(name) p;							\
	static_assert(sizeof(p) == 1 || sizeof(p) == 2 || sizeof(p) == 4 || sizeof(p) == 8, "unsupported field size"); \
	p = (val);								\
	__asm __volatile (							\
		"mov %1,%%gs:%0"						\
		: "=m" (*(uint32_t*)PCPU_OFFSET(name))				\
		: "r" (p));							\
} while (0)

#endif /* __AMD64_PCPU_H__ */
