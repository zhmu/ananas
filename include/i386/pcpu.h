#ifndef __I386_PCPU_H__
#define __I386_PCPU_H__

/* i386-specific per-cpu structure */
#define MD_PCPU_FIELDS \
	void		*context; \
	uint32_t	lapic_id;

#endif /* __I386_PCPU_H__ */
