#include <ananas/types.h>
#include <machine/param.h>
#include <machine/vm.h>
#include <machine/interrupts.h>
#include <machine/macro.h>
#include <machine/mmu.h>
#include <ananas/handle.h>
#include <ananas/mm.h>
#include <ananas/pcpu.h>
#include <ananas/lib.h>
#include <ofw.h>

static uint32_t ofw_msr;
static uint32_t ofw_sprg[4];

struct PCPU bsp_pcpu;
struct STACKFRAME bsp_sf;
static ofw_entry_t ofw_entry;

static uint32_t orig_msr;
static uint32_t orig_sprg0;
static uint32_t orig_sr[PPC_NUM_SREGS];

int
ofw_call(void* arg)
{
	int retval;

	/* Save our MSR and restore the OFW one */
	orig_msr = rdmsr();
	wrmsr(ofw_msr);

	/* Save our SPRG's - we only care about %sprg0 for now */
	__asm __volatile("mfsprg0 %0" : "=r" (orig_sprg0));

	/* FIXME The following causes OFW to hang... need to investigate why */
#ifdef NOTYET
	/* ...and restore the OFW ones */
	__asm __volatile(
		"mtsprg0 %0\n"
		"mtsprg1 %1\n"
		"mtsprg2 %2\n"
		"mtsprg3 %3\n"
	: : "r" (ofw_sprg[0]), "r" (ofw_sprg[1]),
	    "r" (ofw_sprg[2]), "r" (ofw_sprg[3]));
#endif
	__asm __volatile("isync");

	retval = ofw_entry(arg);

	/* Restore our SPRG */
	__asm __volatile("mtsprg0 %0" : : "r" (orig_sprg0));

	/* And restore our MSR */
	wrmsr(orig_msr);

	return retval;
}

void
md_startup(uint32_t r3, uint32_t r4, uint32_t r5)
{
	/*
	 * Save the OpenFirmware entry point and machine registers; it'll cry and
	 * tumble if we do not restore them.
	 */
	ofw_msr = rdmsr();
	__asm __volatile("mfsprg0 %0" : "=r" (ofw_sprg[0]));
	__asm __volatile("mfsprg1 %0" : "=r" (ofw_sprg[1]));
	__asm __volatile("mfsprg2 %0" : "=r" (ofw_sprg[2]));
	__asm __volatile("mfsprg3 %0" : "=r" (ofw_sprg[3]));
	ofw_entry = (ofw_entry_t)r5;

	/* Initial OpenFirmware, this will make kprintf() work */
	ofw_init();
	TRACE("OFW entry point is 0x%x\n", ofw_entry);

	/* XXX As of now, there's no support for real-mode mapped OpenFirmware */
	if ((ofw_msr & (MSR_DR | MSR_IR)) != (MSR_DR | MSR_IR))
		panic("OpenFirmware isn't page-mapped");

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

	/* Initialize the handles; this is needed by the per-CPU code as it initialize threads */
	handle_init();

	/*
	 * Initialize the per-CPU data; this needs a working memory allocator
	 * so we can't do it before here
	 */
	pcpu_init(&bsp_pcpu);

#if 0
	/* XXX we should obtain this info from the loader - note that the OFW map will already
	 * have removed the kernel from the memory map */
	kmem_mark_used((void*)0x00100000, 32);
#endif

	/* Initialize the timer */
	timer_init();

	/* Do the machine independant stuff */
	mi_startup();
}

/* vim:set ts=2 sw=2: */
