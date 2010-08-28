#ifndef __POWERPC_PCPU_H__
#define __POWERPC_PCPU_H__

/* powerpc-specific per-cpu structure */
#define MD_PCPU_FIELDS									\
	/* current thread context */							\
	void*		context;							\
	/* temp_r1 is the field we use to temporarily store %r1 during interrupts */	\
	uint32_t	temp_r1;

#define PCPU_TYPE(x) \
	__typeof(((struct PCPU*)0)->x)

#define PCPU_OFFSET(x) \
	__builtin_offsetof(struct PCPU, x)

#define PCPU_GET(name) ({							\
	PCPU_TYPE(name) p;							\
	addr_t pcpu_base;							\
	__asm __volatile("mfsprg0 %0" : "=r" (pcpu_base) :);			\
	p = *(PCPU_TYPE(name)*)(pcpu_base + PCPU_OFFSET(name));			\
	p;									\
})
	
#define PCPU_SET(name, val) ({							\
	PCPU_TYPE(name)* p;							\
	addr_t pcpu_base;							\
	__asm __volatile("mfsprg0 %0" : "=r" (pcpu_base) :);			\
	p = (PCPU_TYPE(name)*)(pcpu_base + PCPU_OFFSET(name));			\
	*p = (val);								\
})

#endif /* __POWERPC_PCPU_H__ */
