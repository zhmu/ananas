#ifndef __ARM_PCPU_H__
#define __ARM_PCPU_H__

/* powerpc-specific per-cpu structure */
#define MD_PCPU_FIELDS									\
	/* current thread context */							\
	void*		context;							\

extern struct PCPU cpu_pcpu;

#define PCPU_GET(name) cpu_pcpu.name
#define PCPU_SET(name,val) cpu_pcpu.name = (val)

#endif /* __ARM_PCPU_H__ */
