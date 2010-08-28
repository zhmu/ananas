#include <sys/types.h>
#include <machine/macro.h>
#include <machine/vm.h>
#include <machine/param.h>
#include <sys/thread.h>
#include <sys/lib.h>
#include <ofw.h>

#define OFW_MAX_AVAIL 32
static struct ofw_reg ofw_avail[OFW_MAX_AVAIL];

struct ofw_translation {
	ofw_cell_t	virt;
	ofw_cell_t	size;	
	ofw_cell_t	phys;
	ofw_cell_t	mode;
} __attribute__((packed));

static uint8_t vsid_list[MAX_VSIDS	/ 8];

size_t ppc_memory_total;
static addr_t pagemap_addr;
static uint32_t pteg_count; /* number of PTEGs */
static uint8_t htabmask;
static struct PTEG* pteg;
struct BAT bat[16];

extern struct STACKFRAME bsp_sf;
int mmu_debug = 0;

static uint32_t
mmu_alloc_vsid()
{
	static uint32_t curvsid = 0x1000;
/*
	while (1) {
		if ((vsid_list[curvsid / 8] & (1 << (curvsid & 7))) == 0)
			break;
		vsid_list[curvsid / 8] |= (1 << (curvsid & 7));
		curvsid++;
	}*/
		curvsid++;
	return curvsid;
}

void
mmu_map(struct STACKFRAME* sf, uint32_t va, uint32_t pa)
{
	if (sf->sf_sr[va >> 28] == INVALID_VSID)
		sf->sf_sr[va >> 28] = mmu_alloc_vsid() | SR_Kp;
	uint32_t vsid = sf->sf_sr[va >> 28] & 0xffffff;

	for (int hashnum = 0; hashnum < 2; hashnum++) {
		uint32_t htaborg = (addr_t)pteg >> 16;
		uint16_t page = (va >> 12) & 0xffff;
		uint32_t h = vsid ^ page;
		if (hashnum)
			h = (~h) & 0xfffff;
		uint16_t v = (((h >> 10) & 0x1ff) & htabmask) | (htaborg & 0x1ff);
		uint32_t ph = (((htaborg >> 9) & 0x7f) << 25) | (v << 16) | ((h & 0x3ff) << 6);
		if (mmu_debug)
			kprintf("va = %x ==> vsid = %x, api = %x, hash = %x -> ptr=%x\n",
			 va, vsid, page, h, ph);

		struct PTEG* ptegs = (struct PTEG*)ph;
		for (uint32_t i = 0; i < 8; i++) {
			if (ptegs->pte[i].pt_hi & PT_HI_V)
				continue;
			ptegs->pte[i].pt_hi = (vsid << 7) | (page >> 10);
			if (hashnum)
				ptegs->pte[i].pt_hi |= PT_HI_H;
			ptegs->pte[i].pt_lo   = pa;
			ptegs->pte[i].pt_lo  |= 2; // PP
			__asm __volatile("sync");
			ptegs->pte[i].pt_hi |= PT_HI_V;
			return;
		}
	}
	panic("mmu_map: pteg for vsid %x=, va=%x full", vsid, va);
}

void
mmu_map_kernel(struct STACKFRAME* sf)
{
	extern void* __end;
	uint32_t kern_base = 0x0100000;
	uint32_t kern_length = (((addr_t)&__end - kern_base) | (PAGE_SIZE - 1)) + 1;
	TRACE("mmu_map_kernel: mapping %p - %p\n", kern_base, kern_base + kern_length);
	for (int i = 0; i < kern_length; i += PAGE_SIZE) {
		mmu_map(sf, kern_base, kern_base);
		kern_base += PAGE_SIZE;
	}
}

void
mmu_init()
{
	struct ofw_reg ofw_total[OFW_MAX_AVAIL];

	/*
	 * The PowerPC memory management consists of two major mechanisms:
	 *
	 * 1) Block Address Translation (BAT)
	 *    BAT is used to map an adjenct block of memory that is large than a
	 *    single page. This is very useful for bootstrapping and device memory.
	 *
	 * 2) Page Address Translation (PAT)
	 *    PAT can map any page to any other page. It is used for thread memory.
	 *
	 * We use BAT to map the initial 256MB of memory while we are bootstrapping
	 * the system; this will do as our kernel lives there.  This mapping is only
	 * valid in supervisor mode, for instructions and data.
	 *
	 * First of all, enable the recoverable interrupt bits and disable the
	 * instruction/data translation bits. These will only harm us while we are
	 * reprogramming the MMU.
	 */
	wrmsr((rdmsr() & ~(MSR_DR | MSR_IR)) | MSR_RI);

	/* Handle initial 256MB mapping; our kernel lives here */
	memset(bat, 0, sizeof(bat));
	bat[0x0].bat_u = BAT_U(0x000000000, BAT_BL_256MB, BAT_Vs);
	bat[0x0].bat_l = BAT_L(0x000000000, BAT_M, BAT_PP_RW);
	__asm(
		"mtibatu 0,%0; mtibatl 0,%1; isync;"
		"mtdbatu 0,%0; mtdbatl 0,%1; isync;"
	: : "r" (bat[0].bat_u), "r" (bat[0].bat_l));

	/* PCI memory */
	bat[0x8].bat_u = BAT_U(0x80000000, BAT_BL_256MB, BAT_Vs);
	bat[0x8].bat_l = BAT_L(0x80000000, BAT_I | BAT_G, BAT_PP_RW);
	bat[0x9].bat_u = BAT_U(0x90000000, BAT_BL_256MB, BAT_Vs);
	bat[0x9].bat_l = BAT_L(0x90000000, BAT_I | BAT_G, BAT_PP_RW);
	bat[0xa].bat_u = BAT_U(0xa0000000, BAT_BL_256MB, BAT_Vs);
	bat[0xa].bat_l = BAT_L(0xa0000000, BAT_I | BAT_G, BAT_PP_RW);

	/* OBIO */
	bat[0xf].bat_u = BAT_U(0xf0000000, BAT_BL_256MB, BAT_Vs);
	bat[0xf].bat_l = BAT_L(0xf0000000, BAT_I | BAT_G, BAT_PP_RW);

	/*
	 * Enable DBAT translations - by now, we will have set up the interrupt code so
	 * that kernel data faults will cause the corresponding BAT entry to be loaded.
	 * This is required as there are not enough BAT entries available to handle all
	 * mappings simultaneously.
	 */
	wrmsr(rdmsr() | MSR_DR | MSR_IR);

	/* Fetch the memory phandle; we need this to obtain our memory map */
	ofw_cell_t i_memory;
	ofw_cell_t chosen = ofw_finddevice("/chosen");
	ofw_getprop(chosen, "memory", &i_memory, sizeof(i_memory));
	ofw_cell_t p_memory = ofw_instance_to_package(i_memory);

	/*
	 * Figure out the total amount of memory we have; this is needed to determine
	 * the size of our pagetables.
	 */
	unsigned int totallen = ofw_getprop(p_memory, "reg", ofw_total, sizeof(struct ofw_reg) * OFW_MAX_AVAIL);
	for (unsigned int i = 0; i < totallen / sizeof(struct ofw_reg); i++) {
	  ppc_memory_total += ofw_total[i].size;
	}

	/*
	 * In the PowerPC architecture, the page map is always of a fixed size.
	 * However, this size is determined by specifying how many bits are used in
	 * masking addresses. We follow Table 7-9 of "Programming Environments Manual
	 * for 32-Bit Implementations of the PowerPC Architecture", which gives the
	 * recommended minimum values.
	 */

	/* Wire for 8MB initially */
	uint32_t mem_size = ppc_memory_total;
	pteg_count = 1024;
	if (mem_size < 8 * 1024 * 1024)
		return;
	mem_size >>= 24;
	for(; mem_size > 0; pteg_count <<= 1, mem_size >>= 1) ;
	size_t pteg_size = pteg_count * sizeof(struct PTEG);
	TRACE("mmu_init(): %u MB memory, %u PTEG's (%u bytes total)\n",
	 ppc_memory_total / (1024 * 1024), pteg_count, pteg_size);

	/*
	 * OK; now get the free memory map and place the page table in the first
	 * available entry.
	 */
	unsigned int availlen = ofw_getprop(p_memory, "available", ofw_avail, sizeof(struct ofw_reg) * OFW_MAX_AVAIL);
	pteg = NULL;
	for (unsigned int i = 0; i < availlen / sizeof(struct ofw_reg); i++) {
		struct ofw_reg* reg = &ofw_avail[i];
		if (reg->size < pteg_size || reg->base < 0x0100000)
			continue;
		pteg = (struct PTEG*)reg->base;
		if (((addr_t)pteg & 0xffff) > 0) {
			/* Handle alignment */
			uint32_t align = 0x10000 - ((addr_t)pteg & 0xffff);
			pteg = (struct PTEG*)((addr_t)pteg + align); pteg_size += align;
		}
		reg->base += pteg_size; reg->size -= pteg_size;
		break;
	}
	if (pteg == NULL)
		panic("could not find memory for pteg");

	/* Grab the memory and blank it */
	TRACE("Allocated pteg @ %p - %p\n", (addr_t)pteg, (addr_t)pteg + pteg_size);
	KASSERT(((addr_t)pteg & 0xffff) == 0, "PTEG alignment failure");
	memset(pteg, 0, pteg_count * sizeof(struct PTEG));
	htabmask = ((pteg_count * sizeof(struct PTEG)) >> 16) - 1;

	/* Clear the VSID list; they will be allocated by mmu_map() as needed */
	memset(&vsid_list, 0, sizeof(vsid_list));

	/*
	 * OpenFirmware will have allocated a series of memory mappings, which we
	 * will have to copy - if we don't do this, any OFW call will tumble over
	 * and die. Note that this is an PowerPC-specific OFW extension.
	 */
	ofw_cell_t i_mmu;
	ofw_getprop(chosen, "mmu",  &i_mmu,  sizeof(i_mmu));
	ofw_cell_t p_mmu = ofw_instance_to_package(i_mmu);

	/* Use whatever first memory is available for the temporary mmu translations */
	struct ofw_translation* translation = (struct ofw_translation*)ofw_avail[0].base;
	unsigned int translen = ofw_getprop(p_mmu, "translations", translation, 65536);
	if (translen == -1)
		panic("Cannot retrieve OFW translations");
	for (int i = 0; i < translen / sizeof(struct ofw_translation); i++) {
		/*
		 * If the mapping is 1:1, our BAT entries should cover it; this should
		 * apply to device memory. Note that we page in BAT entries as needed
		 * for the OpenFirmware (the same mappings are useful for accessing device
		 * data later on)
		 */
		if (translation[i].phys == translation[i].virt)
			continue;

		/* If the mapping is not at least a page, ignore it */
		if (translation[i].size < PAGE_SIZE)
			continue;

		/*
		 * OK, we should add a mapping for the Virtual->Physical memory. This should
		 * allow us to call OpenFirmware once we have fired up paging.
		 */
		TRACE("ofw mapping %u: phys %x->virt %x (%x bytes), mode=%x\n",
			i,
			translation[i].phys, translation[i].virt,
			translation[i].size, translation[i].mode);
		translation[i].size /= PAGE_SIZE;
		while (translation[i].size > 0) {
			mmu_map(&bsp_sf, translation[i].virt, translation[i].phys);
			translation[i].virt += PAGE_SIZE; translation[i].phys += PAGE_SIZE;
			translation[i].size--;
		}
	}

	/*
	 * Map the kernel space; this mapping will not be used until we deactive the BAT
	 * mapping covering the first 256MB of memory (where the kernel lives).
	 */
	mmu_map_kernel(&bsp_sf);

	/* Map the PTEG map too, we will be needing this to map more memory later */
	uint32_t pteg_addr = (addr_t)pteg;
	for (size_t i = 0; i < pteg_size; i += PAGE_SIZE) {
		mmu_map(&bsp_sf, pteg_addr, pteg_addr);
		pteg_addr += PAGE_SIZE;
	}

	/* It's time to... throw the switch! Really, let's activate the pagetable */
	uint32_t htab = (addr_t)pteg | (((pteg_count * sizeof(struct PTEG)) >> 16) - 1);
	TRACE("activating htab (%x)\n", htab);
	__asm("mtsdr1 %0" : : "r" (htab));

	/* Reload the segment registers  */
	for (int i = 0; i < PPC_NUM_SREGS; i++)
		mtsrin(i << 28, bsp_sf.sf_sr[i]);
	__asm __volatile("sync");

	/* Throw away our BAT mapping; this will allow us to catch NULL pointers */
	bat[0x0].bat_u = 0;
	bat[0x0].bat_l = 0;
	__asm(
		"mtibatu 0,%0; mtibatl 0,%1; isync;"
		"mtdbatu 0,%0; mtdbatl 0,%1; isync;"
	: : "r" (bat[0].bat_u), "r" (bat[0].bat_l));

	/*
	 * Our MMU is completely fired up; now we can add all regions of memory that
	 * the OFW reported.
	 */
	for (unsigned int i = 0; i < availlen / sizeof(struct ofw_reg); i++) {
		struct ofw_reg* reg = &ofw_avail[i];

		TRACE("memory region %u: base=0x%x, size=0x%x\n",
		 i, reg->base, reg->size);

		uint32_t base = reg->base;
		uint32_t size = reg->size;
		base  = (base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
		size &= ~(PAGE_SIZE - 1);
		mm_zone_add(base, size);
	}
}

/* vim:set ts=2 sw=2: */
