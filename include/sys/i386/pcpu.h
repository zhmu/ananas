#ifndef __I386_PCPU_H__
#define __I386_PCPU_H__

/* i386-specific per-cpu structure */
#define MD_PCPU_FIELDS \
	void		*context; \
	uint32_t	lapic_id; \
	addr_t		tss; \
	void		*fpu_context;

#define PCPU_TYPE(x) \
	__typeof(((struct PCPU*)0)->x)

#define PCPU_OFFSET(x) \
	__builtin_offsetof(struct PCPU, x)

#define PCPU_GET(name) ({							\
	PCPU_TYPE(name) p;							\
	if (sizeof(p) != 1 && sizeof(p) != 2 && sizeof(p) != 4)			\
		panic("PCPU_GET: Unsupported field size %u", sizeof(p));	\
	__asm __volatile (							\
		"mov %%fs:%1, %0"						\
		: "=r" (p)							\
		: "m" (*(uint32_t*)(PCPU_OFFSET(name)))				\
	);									\
	p;									\
})

#define PCPU_SET(name, val) do {						\
	PCPU_TYPE(name) p;							\
	p = (val);								\
	if (sizeof(p) != 1 && sizeof(p) != 2 && sizeof(p) != 4)			\
		panic("PCPU_SET: Unsupported field size %u", sizeof(p));	\
	__asm __volatile (							\
		"mov %1,%%fs:%0"						\
		: "=m" (*(uint32_t*)PCPU_OFFSET(name))				\
		: "r" (p));							\
} while (0)

#endif /* __I386_PCPU_H__ */
