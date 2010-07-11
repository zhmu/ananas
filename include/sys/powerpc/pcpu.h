#ifndef __POWERPC_PCPU_H__
#define __POWERPC_PCPU_H__

/* powerpc-specific per-cpu structure */
#define MD_PCPU_FIELDS									\
	/* current thread context */							\
	void*		context;							\
	/* temp_r1 is the field we use to temporarily store %r1 during interrupts */	\
	uint32_t	temp_r1;

#define PCPU_GET(name) \
	NULL

#define PCPU_SET(name, val) \
	0

#endif /* __POWERPC_PCPU_H__ */
