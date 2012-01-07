#ifndef __ARM_THREAD_H__
#define __ARM_THREAD_H__

#include <ananas/types.h>

struct CONTEXT {
};

#define MD_THREAD_FIELDS \
	struct CONTEXT	md_ctx;

#define md_cpu_relax()

#endif /* __ARM_THREAD_H__ */
