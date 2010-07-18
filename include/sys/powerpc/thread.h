#include <sys/types.h>
#include <machine/frame.h>

#ifndef __POWERPC_THREAD_H__
#define __POWERPC_THREAD_H__

/* powerpc-pc thread context */
struct CONTEXT {
	struct		STACKFRAME sf;
};

/* powerpc-specific thread details */
#define MD_THREAD_FIELDS \
	struct CONTEXT	md_ctx; \
	void*		md_stack;


#endif /* __POWERPC_THREAD_H__ */
