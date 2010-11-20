#include <ananas/types.h>
#include <machine/param.h>
#include <machine/vm.h>
#include <machine/interrupts.h>
#include <machine/hal.h>
#include <machine/macro.h>
#include <machine/timer.h>
#include <machine/mmu.h>
#include <ananas/handle.h>
#include <ananas/init.h>
#include <ananas/mm.h>
#include <ananas/pcpu.h>
#include <ananas/lib.h>
#include <ofw.h>
#include "options.h"

struct PCPU bsp_pcpu;
struct STACKFRAME bsp_sf;

void
md_startup(uint32_t r3, uint32_t r4, uint32_t r5)
{
#ifdef OFW
	/* Initialize OpenFirmware; this will enable us to have a working console */
	ofw_md_init(r5);
#endif

	/*
	 * Set up enough per-CPU data to get exceptions running and initialize them.
	 * This will help us debug things if things go bad.
	 */
	memset(&bsp_sf, 0, sizeof(struct STACKFRAME));
	for (int i = 0; i < PPC_NUM_SREGS; i++)
		bsp_sf.sf_sr[i] = INVALID_VSID;
	bsp_pcpu.context = &bsp_sf;
	__asm("mtsprg0 %0" : : "r" (&bsp_pcpu));
	exception_init();

	/* Fire up the MMU; this will initialize the memory manager as well */
	mmu_init();

	/* Allow the HAL to perform post-MMU actions */
	hal_init_post_mmu();

	/* Initialize the handles; this is needed by the per-CPU code as it initialize threads */
	handle_init();

	/*
	 * Initialize the per-CPU data; this needs a working memory allocator
	 * so we can't do it before here
	 */
	pcpu_init(&bsp_pcpu);

	/* Initialize the interrupt code */
	hal_init_interrupts();

	/* Initialize the timer */
	timer_init();

	/* Do the final late initialization */
	hal_init_late();

	/* Do the machine independant stuff */
	mi_startup();
}

/* vim:set ts=2 sw=2: */
