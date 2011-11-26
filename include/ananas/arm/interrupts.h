#ifndef __ARM_INTERRUPTS_H__
#define __ARM_INTERRUPTS_H__

#define MAX_IRQS 16 /* XXX wild guess */

/* XXX This is a no-op stub */
static inline void md_interrupts_enable()
{
}

static inline void md_interrupts_disable()
{
}

static inline void md_interrupts_restore(int enabled)
{
}

static inline int md_interrupts_save()
{
	return 0;
}

static inline int md_interrupts_save_and_disable()
{
	return 0;
}

#endif /* __ARM_INTERRUPTS_H__ */
