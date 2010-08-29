#include <sys/types.h>

#ifndef __POWERPC_FRAME_H__
#define __POWERPC_FRAME_H__

#define PPC_NUM_REGS	32
#define PPC_NUM_SREGS	16

/* powerpc-pc stack frame */
struct STACKFRAME {
	uint32_t	sf_reg[PPC_NUM_REGS];
	uint32_t	sf_xer;
	uint32_t	sf_ctr;
	uint32_t	sf_exc;
	uint32_t	sf_lr;
	uint32_t	sf_cr;
	uint32_t	sf_sr[PPC_NUM_SREGS];
	uint32_t	sf_srr0;
	uint32_t	sf_srr1;
	uint32_t	sf_dar;
	uint32_t	sf_dsisr;
};

#endif /* __POWERPC_FRAME_H__ */
