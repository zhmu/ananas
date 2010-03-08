#include "machine/types.h"

#ifndef __I386_FRAME_H__
#define __I386_FRAME_H__

struct STACKFRAME {
	uint32_t	sf_trapno;
	uint32_t	sf_eax;
	uint32_t	sf_ebx;
	uint32_t	sf_ecx;
	uint32_t	sf_edx;
	uint32_t	sf_ebp;
	uint32_t	sf_esp;
	uint32_t	sf_edi;
	uint32_t	sf_esi;

	uint32_t	sf_ds;
	uint32_t	sf_es;
	uint32_t	sf_fs;
	uint32_t	sf_gs;

	/* added by the hardware */
	register_t	sf_errnum;
	register_t	sf_eip;
	register_t	sf_cs;
	register_t	sf_eflags;
	register_t	sf_sp;
	register_t	sf_ss;
};

#endif /* __I386_FRAME_H__ */
