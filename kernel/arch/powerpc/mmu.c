#include <ananas/types.h>
#include <machine/macro.h>
#include <machine/hal.h>
#include <machine/vm.h>
#include <machine/mmu.h>
#include <machine/param.h>
#include <ananas/thread.h>
#include <ananas/trace.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ofw.h>
#include "options.h"

TRACE_SETUP;

static uint8_t vsid_list[MAX_VSIDS	/ 8];

static uint32_t pteg_count; /* number of PTEGs */
static uint8_t htabmask;
static struct PTEG* pteg;
struct BAT bat[16];

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

int
mmu_unmap(struct STACKFRAME* sf, uint32_t va)
{
	if (sf->sf_sr[va >> 28] == INVALID_VSID) {
		/* entry was never mapped */
		return 0;
	}
	uint32_t vsid = sf->sf_sr[va >> 28] & 0xffffff;

	for (int hashnum = 0; hashnum < 2; hashnum++) {
		uint32_t htaborg = (addr_t)pteg >> 16;
		uint16_t page = (va >> 12) & 0xffff;
		uint32_t h = vsid ^ page;
		if (hashnum)
			h = (~h) & 0xfffff;
		uint16_t v = (((h >> 10) & 0x1ff) & htabmask) | (htaborg & 0x1ff);
		uint32_t ph = (((htaborg >> 9) & 0x7f) << 25) | (v << 16) | ((h & 0x3ff) << 6);

		struct PTEG* ptegs = (struct PTEG*)ph;
		for (uint32_t i = 0; i < 8; i++) {
			if ((ptegs->pte[i].pt_hi & PT_HI_V) == 0)
				continue;
			if (ptegs->pte[i].pt_hi != (PT_HI_V | (vsid << 7) | (page >> 10) | ((hashnum) ? PT_HI_H : 0)))
				continue;
			ptegs->pte[i].pt_hi &= ~PT_HI_V;
			__asm __volatile("sync");
			__asm __volatile("tlbie %0" : : "r" (&ptegs->pte[i]));
			__asm __volatile("sync");
			__asm __volatile("tlbsync");
			__asm __volatile("sync");
			return 1;
		}
	}
	return 0;
}

addr_t
mmu_resolve_mapping(struct STACKFRAME* sf, uint32_t va, int flags)
{
	if (sf->sf_sr[va >> 28] == INVALID_VSID)
		return 0;
	uint32_t vsid = sf->sf_sr[va >> 28] & 0xffffff;

	for (int hashnum = 0; hashnum < 2; hashnum++) {
		uint32_t htaborg = (addr_t)pteg >> 16;
		uint16_t page = (va >> 12) & 0xffff;
		uint32_t h = vsid ^ page;
		if (hashnum)
			h = (~h) & 0xfffff;
		uint16_t v = (((h >> 10) & 0x1ff) & htabmask) | (htaborg & 0x1ff);
		uint32_t ph = (((htaborg >> 9) & 0x7f) << 25) | (v << 16) | ((h & 0x3ff) << 6);

		struct PTEG* ptegs = (struct PTEG*)ph;
		for (uint32_t i = 0; i < 8; i++) {
			if ((ptegs->pte[i].pt_hi & PT_HI_V) == 0)
				continue;
			if (ptegs->pte[i].pt_hi != (PT_HI_V | (vsid << 7) | (page >> 10) | ((hashnum) ? PT_HI_H : 0)))
				continue;
			/* XXX check flags */
			return ptegs->pte[i].pt_lo & ~(PAGE_SIZE - 1);
		}
	}
	return 0;
}

void
mmu_map_kernel(struct STACKFRAME* sf)
{
	extern void* __end;
	uint32_t kern_base = 0x0100000;
	uint32_t kern_length = (((addr_t)&__end - kern_base) | (PAGE_SIZE - 1)) + 1;
	TRACE(MACHDEP, INFO, "mmu_map_kernel: mapping %p - %p\n", kern_base, kern_base + kern_length);
	for (int i = 0; i < kern_length; i += PAGE_SIZE) {
		mmu_map(sf, kern_base, kern_base);
		kern_base += PAGE_SIZE;
	}
}

void
mmu_init()
{
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

	/*
	 * Initialize memory-aspects; this should cause our corresponding HAL to
	 * query memory maps and the like. It will return the total amount of
	 * memory found (in bytes), which we need to size the pagetables.
	 */
	size_t memory_total = hal_init_memory();

	/*
	 * In the PowerPC architecture, the page map is always of a fixed size.
	 * However, this size is determined by specifying how many bits are used in
	 * masking addresses. We follow Table 7-9 of "Programming Environments Manual
	 * for 32-Bit Implementations of the PowerPC Architecture", which gives the
	 * recommended minimum values.
	 */

	/* Wire for 8MB initially */
	uint32_t mem_size = memory_total;
	pteg_count = 1024;
	mem_size >>= 24;
	for(; mem_size > 0; pteg_count <<= 1, mem_size >>= 1) ;
	size_t pteg_size = pteg_count * sizeof(struct PTEG);
	TRACE(MACHDEP, INFO, "mmu_init(): %u MB memory, %u PTEG's (%u bytes total)\n",
	 memory_total / (1024 * 1024), pteg_count, pteg_size);

	/*
	 * Find a location for the PTEG; this is done by traversing the memory map
	 * obtained by the HAL and using the first available entry.
	 */
	struct HAL_REGION* hal_avail; int hal_num_avail;
	hal_get_available_memory(&hal_avail, &hal_num_avail);
	for (unsigned int i = 0; i < hal_num_avail; i++) {
		TRACE(MACHDEP, INFO, "mmu_init(): hal_avail[%i]: reg_size=%u, base=0x%x\n",
		 i, hal_avail[i].reg_size, hal_avail[i].reg_base);
		/* Ensure we never use a NULL pointer */
		if (hal_avail[i].reg_base == 0) {
			hal_avail[i].reg_base += PAGE_SIZE;
			hal_avail[i].reg_size -= PAGE_SIZE;
		}
		if (hal_avail[i].reg_size < pteg_size)
			continue;
		pteg = (struct PTEG*)hal_avail[i].reg_base;
		if (((addr_t)pteg & 0xffff) > 0) {
			/* Handle alignment */
			uint32_t align = 0x10000 - ((addr_t)pteg & 0xffff);
			pteg = (struct PTEG*)((addr_t)pteg + align); pteg_size += align;
		}
		hal_avail[i].reg_base += pteg_size; hal_avail[i].reg_size -= pteg_size;
		break;
	}
	if (pteg == NULL)
		panic("could not find memory for pteg");

	/* Grab the memory and blank it */
	TRACE(MACHDEP, INFO, "allocated pteg @ %p - %p\n", (addr_t)pteg, (addr_t)pteg + pteg_size);
	KASSERT(((addr_t)pteg & 0xffff) == 0, "PTEG alignment failure");
	memset(pteg, 0, pteg_count * sizeof(struct PTEG));
	htabmask = ((pteg_count * sizeof(struct PTEG)) >> 16) - 1;

	/* Clear the VSID list; they will be allocated by mmu_map() as needed */
	memset(&vsid_list, 0, sizeof(vsid_list));

#ifdef OPTION_OFW
	/*
	 * OpenFirmware will have allocated a series of memory mappings, which we
	 * will have to copy - if we don't do this, any OFW call will tumble over
	 * and die. Note that this is an PowerPC-specific OFW extension.
	 */
	ofw_md_setup_mmu();
#endif /* OPTION_OFW */

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
	TRACE(MACHDEP, INFO, "activating htab (%x)\n", htab);
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
	hal_get_available_memory(&hal_avail, &hal_num_avail);
	mm_init();
	for (unsigned int i = 0; i < hal_num_avail; i++) {
		TRACE(MACHDEP, INFO, "memory region %u: base=0x%x, size=0x%x\n",
		 i, hal_avail[i].reg_base, hal_avail[i].reg_size);

		uint32_t base = hal_avail[i].reg_base;
		uint32_t size = hal_avail[i].reg_size;
		base  = (base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
		size &= ~(PAGE_SIZE - 1);
		mm_zone_add(base, size);
	}
}

/* vim:set ts=2 sw=2: */
