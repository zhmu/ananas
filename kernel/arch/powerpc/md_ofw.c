#include <ananas/types.h>
#include <ananas/lib.h>
#include <ananas/trace.h>
#include <machine/frame.h>
#include <machine/macro.h>
#include <machine/mmu.h>
#include <machine/param.h>
#include <machine/vm.h>
#include <ofw.h>

TRACE_SETUP;

static uint32_t ofw_msr;
static uint32_t ofw_sprg[4];
static ofw_entry_t ofw_entry;

static uint32_t orig_msr;
static uint32_t orig_sprg0;

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
ofw_md_init(register_t entry)
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
	ofw_entry = (ofw_entry_t)entry;

	/* Initial OpenFirmware I/O, this will make kprintf() work */
	ofw_init_io();
	TRACE(MACHDEP, INFO, "OFW entry point is 0x%x\n", ofw_entry);

	/* XXX As of now, there's no support for real-mode mapped OpenFirmware */
	if ((ofw_msr & (MSR_DR | MSR_IR)) != (MSR_DR | MSR_IR))
		panic("OpenFirmware isn't page-mapped");
}

void
ofw_md_setup_mmu()
{
#define MAX_TRANSLATIONS 64
	struct ofw_translation ofw_trans[MAX_TRANSLATIONS];

	ofw_cell_t i_mmu;
	ofw_cell_t chosen = ofw_finddevice("/chosen");
	ofw_getprop(chosen, "mmu",  &i_mmu,  sizeof(i_mmu));
	ofw_cell_t p_mmu = ofw_instance_to_package(i_mmu);

	/* Use whatever first memory is available for the temporary mmu translations */
	unsigned int translen = ofw_getprop(p_mmu, "translations", ofw_trans, sizeof(ofw_trans));
	if (translen == -1)
		panic("Cannot retrieve OFW translations");
	for (int i = 0; i < translen / sizeof(struct ofw_translation); i++) {
		/*
		 * If the mapping is 1:1, our BAT entries should cover it; this should
		 * apply to device memory. Note that we page in BAT entries as needed
		 * for the OpenFirmware (the same mappings are useful for accessing device
		 * data later on)
		 */
		if (ofw_trans[i].phys == ofw_trans[i].virt)
			continue;

		/* If the mapping is not at least a page, ignore it */
		if (ofw_trans[i].size < PAGE_SIZE)
			continue;

		/*
		 * OK, we should add a mapping for the Virtual->Physical memory. This should
		 * allow us to call OpenFirmware once we have fired up paging.
		 */
		TRACE(MACHDEP, INFO, "ofw mapping %u: phys %x->virt %x (%x bytes), mode=%x\n",
			i,
			ofw_trans[i].phys, ofw_trans[i].virt,
			ofw_trans[i].size, ofw_trans[i].mode);
		ofw_trans[i].size /= PAGE_SIZE;
		while (ofw_trans[i].size > 0) {
			mmu_map(&bsp_sf, ofw_trans[i].virt, ofw_trans[i].phys);
			ofw_trans[i].virt += PAGE_SIZE; ofw_trans[i].phys += PAGE_SIZE;
			ofw_trans[i].size--;
		}
	}
}

/* vim:set ts=2 sw=2: */
