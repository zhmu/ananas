#include <sys/types.h>
#include <machine/param.h>
#include <machine/vm.h>
#include <machine/interrupts.h>
#include <machine/macro.h>
#include <sys/mm.h>
#include <sys/pcpu.h>
#include <sys/lib.h>
#include <ofw.h>

static uint32_t ofw_msr;
static uint32_t ofw_sprg[4];

struct PCPU bsp_pcpu;
struct STACKFRAME bsp_sf;
static ofw_entry_t ofw_entry;

int
ofw_call(void* arg)
{
	uint32_t orig_msr;
	uint32_t orig_sprg[4];
	int retval;

	/* Save our MSR and restore the OFW one */
	orig_msr = rdmsr();
	wrmsr(ofw_msr);

	/* Save our SPRG's and restore the OFW ones */
	__asm __volatile("mfsprg0 %0; mtsprg0 %1" : "=r" (orig_sprg[0]) : "r" (ofw_sprg[0]));
	__asm __volatile("mfsprg1 %0; mtsprg1 %1" : "=r" (orig_sprg[1]) : "r" (ofw_sprg[1]));
	__asm __volatile("mfsprg2 %0; mtsprg2 %1" : "=r" (orig_sprg[2]) : "r" (ofw_sprg[2]));
	__asm __volatile("mfsprg3 %0; mtsprg3 %1" : "=r" (orig_sprg[3]) : "r" (ofw_sprg[3]));

	retval = ofw_entry(arg);

	/* Restore our SPRG's */
	__asm("mtsprg0 %0" : : "r" (orig_sprg[0]));
	__asm("mtsprg1 %0" : : "r" (orig_sprg[1]));
	__asm("mtsprg2 %0" : : "r" (orig_sprg[2]));
	__asm("mtsprg3 %0" : : "r" (orig_sprg[3]));

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
	__asm("mfsprg0 %0" : "=r" (ofw_sprg[0]));
	__asm("mfsprg1 %0" : "=r" (ofw_sprg[1]));
	__asm("mfsprg2 %0" : "=r" (ofw_sprg[2]));
	__asm("mfsprg3 %0" : "=r" (ofw_sprg[3]));
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

#if 0
	/* XXX we should obtain this info from the loader - note that the OFW map will already
	 * have removed the kernel from the memory map */
	kmem_mark_used((void*)0x00100000, 32);
#endif

	mi_startup();
}

/* vim:set ts=2 sw=2: */
