#include "types.h"
#include "i386/smp.h"
#include "i386/pcpu.h"
#include "i386/apic.h"
#include "i386/macro.h"
#include "i386/thread.h"
#include "i386/vm.h"
#include "lock.h"
#include "param.h"
#include "pcpu.h"
#include "vm.h"
#include "mm.h"
#include "lib.h"

/* Application Processor's entry point and end */
void *__ap_entry, *__ap_entry_end;

void md_map_kernel_lowaddr(uint32_t* pd);
extern uint32_t* pagedir;

struct IA32_CPU** cpus;
uint8_t* lapic2cpuid;
static uint32_t num_cpu;

static struct SPINLOCK spl_smp_launch;

static void
delay(int n) {
	while (n-- > 0) {
		int x = 1000;
		while (x--);
	}
}

static addr_t
locate_mpfps_region(addr_t base, size_t length)
{
	uint32_t* ptr = (uint32_t*)base;

	/*
	 * Every table must reside on a 16-byte boundery, so doing
	 * increments of 4 should work fine.
	 */
	for (; length > 0; length -= 4, ptr++) {
		if (*ptr == MP_FPS_SIGNATURE)
			return (addr_t)ptr;
	}
	return 0;
}

static int
validate_checksum(addr_t base, size_t length)
{
	uint8_t cksum = 0;

	while (length > 0) {
		cksum += *(uint8_t*)base;
		base++; length--;
	}
	return cksum == 0;
}

uint32_t
get_num_cpus()
{
	return num_cpu;
}

struct IA32_CPU*
get_cpu_struct(int i)
{
	return (struct IA32_CPU*)(cpus + (sizeof(struct IA32_CPU*) * num_cpu) + i * sizeof(struct IA32_CPU));
}

static addr_t
locate_mpfps()
{
	addr_t mpfps = 0;

	/*
	 * According to Intel's MultiProcessor specification, the MP Floating Pointer
	 * Structure can be located at:
	 *
	 * 1) The first 1KB of the EBDA
	 * 2) Within the last 1KB of system base memory (639-640KB)
	 * 3) BIOS ROM space 0xF0000 - 0xFFFFF.
	 *
	 * If we don't find it here, this system isn't SMP capable and we give up.
	 */

	/*
	 * The EBDA address can be read from absolute address 0x40e, so fetch it.
	 * While here, figure out the last KB of the base memory too, just in case
	 * we need it later on.
	 */
	vm_map(0, 1);
	uint32_t ebda_addr = (*(uint16_t*)(0x40e)) << 4;
	uint32_t basemem_last = (*(uint16_t*)(0x413) * 1024);
	vm_unmap(0, 1);

	/*
	 * Attempt to locate the MPFP structure in the EBDA memory space. Note that
	 * we map 2 pages to ensure things won't go badly if we cross a paging
	 * boundary.
	 */
	vm_map(ebda_addr & ~(PAGE_SIZE - 1), 2);
	mpfps = locate_mpfps_region(ebda_addr, 1024);
	vm_unmap(ebda_addr & ~(PAGE_SIZE - 1), 2);
	if (mpfps != 0)
		return mpfps;

	/* Attempt to locate the MPFP structure in the last 1KB of base memory */
	vm_map(basemem_last & ~(PAGE_SIZE - 1), 2);
	mpfps = locate_mpfps_region(basemem_last, 1024);
	vm_unmap(basemem_last & ~(PAGE_SIZE - 1), 2);
	if (mpfps != 0)
		return mpfps;

	/* Finally, attempt to locate the MPFP structure in the BIOS address space */
	vm_map(0xf0000, 16);
	mpfps = locate_mpfps_region(0xf0000, 65536);
	vm_unmap(0xf0000, 16);

	return mpfps;
}

/*
 * Called on the Boot Strap Processor, in order to prepare the system for
 * multiprocessing.
 */
void
smp_init()
{
	addr_t mpfps_addr = locate_mpfps();
	if (mpfps_addr == 0)
		return;

	/*
	 * We just copy the MPFPS structure, since it's a fixed length and it
	 * makes it much easier to check requirements.
	 */
	vm_map(mpfps_addr & ~(PAGE_SIZE - 1), 1);
	struct MP_FLOATING_POINTER mpfps;
	memcpy (&mpfps, (struct MP_FLOATING_POINTER*)mpfps_addr, sizeof(struct MP_FLOATING_POINTER));
	vm_unmap(mpfps_addr & ~(PAGE_SIZE - 1), 1);

	/* Verify checksum before we do anything else */
	if (!validate_checksum((addr_t)&mpfps, sizeof(struct MP_FLOATING_POINTER))) {
		kprintf("SMP: mpfps structure corrupted, ignoring\n");
		return;
	}
	if (mpfps.feature1 & 0x7f != 0) {
		kprintf("SMP: predefined configuration %u not implemented\n", mpfps.feature1 & 0x7f);
		return;
	}
	if (mpfps.phys_ptr == 0) {
		kprintf("SMP: no MP configuration table specified\n");
		return;
	}

	/*
	 * Map the first page of the configuration table; this is needed to calculate
	 * the length of the configuration, so we can map the whole thing.
	 */
	vm_map(mpfps.phys_ptr & ~(PAGE_SIZE - 1), 1);
	struct MP_CONFIGURATION_TABLE* mpct = (struct MP_CONFIGURATION_TABLE*)mpfps.phys_ptr;
	uint32_t mpct_signature = mpct->signature;
	uint32_t mpct_numpages = (sizeof(struct MP_CONFIGURATION_TABLE) + mpct->entry_count * 20 + PAGE_SIZE - 1) / PAGE_SIZE;
	vm_unmap(mpfps.phys_ptr & ~(PAGE_SIZE - 1), 1);
	if (mpct_signature != MP_CT_SIGNATURE)
		return;

	vm_map(mpfps.phys_ptr & ~(PAGE_SIZE - 1), mpct_numpages);
	if (!validate_checksum(mpfps.phys_ptr , mpct->base_len)) {
		vm_unmap(mpfps.phys_ptr & ~(PAGE_SIZE - 1), mpct_numpages);
		kprintf("SMP: mpct structure corrupted, ignoring\n");
		return;
	}

	kprintf("SMP: <%c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c%c%c%c%c> version 1.%u\n",
	 mpct->oem_id[0], mpct->oem_id[1], mpct->oem_id[2], mpct->oem_id[3],
	 mpct->oem_id[4], mpct->oem_id[5], mpct->oem_id[6], mpct->oem_id[7],
	 mpct->product_id[0], mpct->product_id[1], mpct->product_id[2],
	 mpct->product_id[3], mpct->product_id[4], mpct->product_id[5],
	 mpct->product_id[6], mpct->product_id[7], mpct->product_id[8],
	 mpct->product_id[9], mpct->product_id[10], mpct->product_id[11],
	 mpct->spec_rev);

	/*
	 * First of all, count the number of entries; this is needed because we
	 * allocate memory for each item as needed.
	 */
	num_cpu = 0;
	int i, num_ioapic = 0;
	int bsp_apic_id = -1, max_apic_id = -1;
	uint16_t num_entries = mpct->entry_count;
	addr_t entry_addr = (addr_t)mpct + sizeof(struct MP_CONFIGURATION_TABLE);
	for (i = 0; i < num_entries; i++) {
		struct MP_ENTRY* entry = (struct MP_ENTRY*)entry_addr;
		switch(entry->type) {
			case MP_ENTRY_TYPE_PROCESSOR:
				/* Only count enabled CPU's */
				if (entry->u.proc.flags & MP_PROC_FLAG_EN)
					num_cpu++;
				if (entry->u.proc.flags & MP_PROC_FLAG_BP)
					bsp_apic_id = entry->u.proc.lapic_id;
				if (entry->u.proc.lapic_id > max_apic_id)
					max_apic_id = entry->u.proc.lapic_id;
				entry_addr += 20;
				break;
			case MP_ENTRY_TYPE_IOAPIC:
				num_ioapic++;
				/* FALLTHROUGH */
			default:
				entry_addr += 8;
				break;
		}
	}
	if (bsp_apic_id == -1)
		panic("smp: no BSP processor defined!\n");

	kprintf("SMP: %u CPU(s) detected, %u IOAPIC(s)\n", num_cpu, num_ioapic);
	if (num_cpu < 2)
		return;

	/*
	 * Prepare the LAPIC ID -> CPU ID array, we use this to quickly look up
	 * the corresponding CPU structures.
	 */
	lapic2cpuid = kmalloc(max_apic_id);

	/* Prepare the CPU structure. CPU #0 is always the BSP */
	cpus = (struct IA32_CPU**)kmalloc((sizeof(struct IA32_CPU*) + sizeof(struct IA32_CPU)) * num_cpu);
	memset(cpus, 0, (sizeof(struct IA32_CPU*) + sizeof(struct IA32_CPU)) * num_cpu);
	for (i = 0; i < num_cpu; i++) {
		cpus[i] = (struct IA32_CPU*)(cpus + (sizeof(struct IA32_CPU*) * num_cpu) + i * sizeof(struct IA32_CPU));

		/* Note that we don't need to allocate anything extra for the BSP */
		if (i == 0)
			continue;

		cpus[i]->stack = kmalloc(KERNEL_STACK_SIZE);
		KASSERT(cpus[i]->stack != NULL, "out of memory?");

		uint32_t admin_length = GDT_NUM_ENTRIES * 8 + sizeof(struct TSS) + sizeof(struct PCPU);
		char* buf = kmalloc(admin_length);

		cpus[i]->gdt = buf;
extern void* gdt;
		memcpy(cpus[i]->gdt, &gdt, GDT_NUM_ENTRIES * 8);
struct TSS* tss = (struct TSS*)(buf + GDT_NUM_ENTRIES * 8);
		memset(tss, 0, sizeof(struct TSS));
		tss->ss0 = GDT_SEL_KERNEL_DATA;
		GDT_SET_TSS(cpus[i]->gdt, GDT_IDX_KERNEL_TASK, 0, (addr_t)tss, sizeof(struct TSS));
		cpus[i]->tss = (char*)tss;
struct PCPU* pcpu = (struct PCPU*)(buf + GDT_NUM_ENTRIES * 8 + sizeof(struct TSS));
		memset(pcpu, 0, sizeof(struct PCPU));
		pcpu->tss = (addr_t)cpus[i]->tss;
		GDT_SET_ENTRY32(cpus[i]->gdt, GDT_IDX_KERNEL_PCPU, SEG_TYPE_DATA, SEG_DPL_SUPERVISOR, (addr_t)pcpu, sizeof(struct PCPU));
	}

	/* Wade through the table */
	int cur_cpu = 0;
	entry_addr = (addr_t)mpct + sizeof(struct MP_CONFIGURATION_TABLE);
	for (i = 0; i < num_entries; i++) {
		struct MP_ENTRY* entry = (struct MP_ENTRY*)entry_addr;
		switch(entry->type) {
			case MP_ENTRY_TYPE_PROCESSOR:
				if (entry->u.proc.flags & MP_PROC_FLAG_EN == 0)
					break;
				kprintf("CPU%u: ID #%u, %x,%x,%x !\n", cur_cpu, entry->u.proc.lapic_id, entry->u.proc.signature, entry->u.proc.features, entry->u.proc.flags);
				lapic2cpuid[entry->u.proc.lapic_id] = cur_cpu;
				cur_cpu++;
				break;
			default:
				break;
		}
		entry_addr += (entry->type == MP_ENTRY_TYPE_PROCESSOR) ? 20 : 8;
	}

	vm_unmap(mpfps.phys_ptr & ~(PAGE_SIZE - 1), mpct_numpages);

	/*
	 * OK, the AP's start in real mode, so we need to provide them with a
	 * stub so they can run in protected mode. This stub must be located
	 * in the lower 1MB...
	 */
	char* ap_code = kmalloc(PAGE_SIZE);
	KASSERT ((addr_t)ap_code < 0x100000, "ap code must be below 1MB"); /* XXX crude */
	memcpy(ap_code, &__ap_entry, (addr_t)&__ap_entry_end - (addr_t)&__ap_entry);

	/* Map the local APIC memory and enable it */
	vm_map_device(LAPIC_BASE, LAPIC_SIZE);
	*((uint32_t*)LAPIC_SVR) |= LAPIC_SVR_APIC_EN;

	/*
	 * Initialize the SMP launch spinlock; every AP will try this as well. Once
	 * we are done, the BSP unlocks, causing every 
	 */
	spinlock_init(&spl_smp_launch);
	spinlock_lock(&spl_smp_launch);

	/*
	 * Broadcast INIT-SIPI-SIPI-IPI to all AP's; this will wake them up and cause
	 * them to run the AP entry code.
	 */
	*((uint32_t*)LAPIC_ICR_LOW) = 0xc4500;	/* INIT */
	delay(10);
	*((uint32_t*)LAPIC_ICR_LOW) = 0xc4600 | (addr_t)ap_code >> 12;	/* SIPI */
	delay(200);
	*((uint32_t*)LAPIC_ICR_LOW) = 0xc4600 | (addr_t)ap_code >> 12;	/* SIPI */
	delay(200);
}

/*
 * Called on the Boot Strap Processor, in order to fully launch the AP's.
 */
void
smp_launch()
{
	spinlock_unlock(&spl_smp_launch);
}

/*
 * Called by mp_stub.S for every Application Processor. Should not return.
 */
void
mp_ap_startup(uint32_t lapic_id)
{
	spinlock_lock(&spl_smp_launch);
	spinlock_unlock(&spl_smp_launch);

	/*
	 * We're up and running!
	 */
	schedule();
}

/* vim:set ts=2 sw=2: */
