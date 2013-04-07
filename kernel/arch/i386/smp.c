#include <ananas/types.h>
#include <machine/smp.h>
#include <machine/pcpu.h>
#include <machine/apic.h>
#include <machine/ioapic.h>
#include <machine/interrupts.h>
#include <machine/macro.h>
#include <machine/thread.h>
#include <machine/vm.h>
#include <ananas/x86/io.h>
#include <ananas/error.h>
#include <ananas/lock.h>
#include <machine/param.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <ananas/vm.h>
#include <ananas/mm.h>
#include <ananas/thread.h>
#include <ananas/trace.h>
#include <ananas/lib.h>

#undef SMP_DEBUG

TRACE_SETUP;

/* Application Processor's entry point and end */
void *__ap_entry, *__ap_entry_end;

void md_remove_low_mappings();
void md_map_kernel_lowaddr(uint32_t* pd);
extern uint32_t* pagedir;

struct IA32_CPU** cpus;
uint8_t* lapic2cpuid;
static uint32_t num_cpu;

struct IA32_IOAPIC** ioapics;
static uint32_t num_ioapic;

struct IA32_BUS** busses;
static uint32_t num_bus;

struct IA32_INTERRUPT** ints;
static uint32_t num_ints;

static char* ap_code;
static int can_smp_launch = 0;
static int num_smp_launched = 1; /* BSP is always launched */

static void ioapic_ack(struct IRQ_SOURCE* source, int no);

static struct IRQ_SOURCE ipi_source = {
	.is_first = SMP_IPI_FIRST,
	.is_count = SMP_IPI_COUNT,
	.is_mask = NULL,
	.is_unmask = NULL,
	.is_ack = ioapic_ack
};

static void
delay(int n) {
	while (n-- > 0) {
		int x = 1000;
		while (x--);
	}
}

static addr_t
locate_mpfps_region(void* base, size_t length)
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
	void* biosinfo_ptr = vm_map_kernel(0, 1, VM_FLAG_READ);
	uint32_t ebda_addr = (*(uint16_t*)((addr_t)biosinfo_ptr + 0x40e)) << 4;
	uint32_t basemem_last = (*(uint16_t*)((addr_t)biosinfo_ptr + 0x413) * 1024);
	vm_unmap_kernel((addr_t)biosinfo_ptr, 1);

	/*
	 * Attempt to locate the MPFP structure in the EBDA memory space. Note that
	 * we map 2 pages to ensure things won't go badly if we cross a paging
	 * boundary.
	 */
	void* ebda_ptr = vm_map_kernel(ebda_addr & ~(PAGE_SIZE - 1), 2, VM_FLAG_READ);
	mpfps = locate_mpfps_region((void*)((addr_t)ebda_ptr + (ebda_addr % PAGE_SIZE)), 1024);
	vm_unmap_kernel((addr_t)ebda_ptr, 2);
	if (mpfps != 0)
		return KVTOP(mpfps);

	/* Attempt to locate the MPFP structure in the last 1KB of base memory */
	void* basemem_ptr = vm_map_kernel(basemem_last & ~(PAGE_SIZE - 1), 2, VM_FLAG_READ);
	mpfps = locate_mpfps_region(basemem_ptr, 1024);
	vm_unmap_kernel((addr_t)basemem_ptr, 2);
	if (mpfps != 0)
		return KVTOP(mpfps);

	/* Finally, attempt to locate the MPFP structure in the BIOS address space */
	void* bios_addr = vm_map_kernel(0xf0000, 16, VM_FLAG_READ);
	mpfps = locate_mpfps_region(bios_addr, 65536);
	vm_unmap_kernel((addr_t)bios_addr, 16);
	if (mpfps != 0)
		return KVTOP(mpfps);

	return 0;
}

/* XXX all ioapic_... functionality doesn't belong here... */
void
ioapic_write(struct IA32_IOAPIC* apic, uint32_t reg, uint32_t val)
{
	*(volatile uint32_t*)(apic->ioa_addr + IOREGSEL) = reg & 0xff;
	*(volatile uint32_t*)(apic->ioa_addr + IOWIN) = val;
}

uint32_t
ioapic_read(struct IA32_IOAPIC* apic, uint32_t reg)
{
	*(volatile uint32_t*)(apic->ioa_addr + IOREGSEL) = reg & 0xff;
	return *(volatile uint32_t*)(apic->ioa_addr + IOWIN);
}

static errorcode_t
ioapic_mask(struct IRQ_SOURCE* source, int no)
{
	panic("not implemented");
}

static errorcode_t
ioapic_unmask(struct IRQ_SOURCE* source, int no)
{
	panic("not implemented");
}

static void
ioapic_ack(struct IRQ_SOURCE* source, int no)
{
	struct IA32_IOAPIC* ioapic = source->is_privdata;
	(void)ioapic;
	*((uint32_t*)LAPIC_EOI) = 0;
}

static int
resolve_bus_type(struct MP_ENTRY_BUS* bus)
{
	struct BUSTYPE {
		char* name;
		int type;
	} bustypes[] = {
		{ "ISA   ", BUS_TYPE_ISA },
		{ NULL, 0 }
	};

	for (struct BUSTYPE* bt = bustypes; bt->name != NULL; bt++) {
		int i;
		for (i = 0; i < sizeof(bus->type) && bus->type[i] == bt->name[i]; i++)
			/* nothing */;
		if (i != sizeof(bus->type))
			continue;

		return bt->type;
	}
	return BUS_TYPE_UNKNOWN;
}

static struct IA32_BUS*
find_bus(int id)
{
	struct IA32_BUS** bus = busses;
	int i;

	for (i = 0; i < num_bus; i++, bus++) {
		if ((*bus)->id == id)
			return *bus;
	}
	return NULL;
}

static struct IA32_IOAPIC*
find_ioapic(int id)
{
	struct IA32_IOAPIC** ioapic = ioapics;

	for (int i = 0; i < num_ioapic; i++, ioapic++) {
		if ((*ioapic)->ioa_id == id)
			return *ioapic;
	}
	return NULL;
}

static void
smp_ipi_schedule(device_t dev)
{
	/* Flip the reschedule flag of the current thread; this makes the IRQ reschedule us as needed */
	thread_t* curthread = PCPU_GET(curthread);
	curthread->t_flags |= THREAD_FLAG_RESCHEDULE;
}

static void
smp_ipi_panic(device_t dev)
{
	while (1) {
		__asm("hlt");
	}
}

/*
 * Called on the Boot Strap Processor, in order to prepare the system for
 * multiprocessing.
 */
errorcode_t
smp_init()
{
	addr_t mpfps_addr = locate_mpfps();
	if (mpfps_addr == 0)
		return ANANAS_ERROR(NO_DEVICE);

	/*
	 * We just copy the MPFPS structure, since it's a fixed length and it
	 * makes it much easier to check requirements.
	 */
	void* mpfps_ptr = vm_map_kernel(mpfps_addr & ~(PAGE_SIZE - 1), 1, VM_FLAG_READ);
	struct MP_FLOATING_POINTER mpfps;
	memcpy(&mpfps, (void*)((addr_t)mpfps_ptr + (mpfps_addr % PAGE_SIZE)), sizeof(struct MP_FLOATING_POINTER));
	vm_unmap_kernel((addr_t)mpfps_ptr, 1);

	/* Verify checksum before we do anything else */
	if (!validate_checksum((addr_t)&mpfps, sizeof(struct MP_FLOATING_POINTER))) {
		kprintf("SMP: mpfps structure corrupted, ignoring\n");
		goto smp_abort;
	}
	if ((mpfps.feature1 & 0x7f) != 0) {
		kprintf("SMP: predefined configuration %u not implemented\n", mpfps.feature1 & 0x7f);
		goto smp_abort;
	}
	if (mpfps.phys_ptr == 0) {
		kprintf("SMP: no MP configuration table specified\n");
		goto smp_abort;
	}

	/*
	 * Map the first page of the configuration table; this is needed to calculate
	 * the length of the configuration, so we can map the whole thing.
	 */
	void* mpfps_phys_ptr = vm_map_kernel(mpfps.phys_ptr & ~(PAGE_SIZE - 1), 1, VM_FLAG_READ);
	struct MP_CONFIGURATION_TABLE* mpct = (struct MP_CONFIGURATION_TABLE*)(mpfps_phys_ptr + (mpfps.phys_ptr % PAGE_SIZE));
	uint32_t mpct_signature = mpct->signature;
	uint32_t mpct_numpages = (sizeof(struct MP_CONFIGURATION_TABLE) + mpct->entry_count * 20 + PAGE_SIZE - 1) / PAGE_SIZE;
	vm_unmap_kernel((addr_t)mpfps_phys_ptr, 1);
	if (mpct_signature != MP_CT_SIGNATURE)
		goto smp_abort;

	mpfps_phys_ptr = vm_map_kernel(mpfps.phys_ptr & ~(PAGE_SIZE - 1), mpct_numpages, VM_FLAG_READ);
	mpct = (struct MP_CONFIGURATION_TABLE*)(mpfps_phys_ptr + (mpfps.phys_ptr % PAGE_SIZE));
	if (!validate_checksum((addr_t)mpct, mpct->base_len)) {
		vm_unmap_kernel((addr_t)mpfps_phys_ptr, mpct_numpages);
		kprintf("SMP: mpct structure corrupted, ignoring\n");
		goto smp_abort;
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
	 * OK, the AP's start in real mode, so we need to provide them with a
	 * stub so they can run in protected mode. This stub must be located
	 * in the lower 1MB.
	 *
	 * Note that the low memory mappings will not be removed in the SMP case, so
	 * that the AP's can correctly switch to protected mode and enable paging.
	 * Once this is all done, the mapping can safely be removed.
	 *
	 * XXX We do this before allocating kernel structures to increase the odds of
	 * the AP code remaining in the lower 1MB. This needs a better solution.
	 */
	ap_code = kmalloc(PAGE_SIZE);
	KASSERT (KVTOP((addr_t)ap_code) < 0x100000, "ap code %p must be below 1MB"); /* XXX crude */
	memcpy(ap_code, &__ap_entry, (addr_t)&__ap_entry_end - (addr_t)&__ap_entry);

	/*
	 * First of all, count the number of entries; this is needed because we
	 * allocate memory for each item as needed.
	 */
	num_cpu = 0; num_ioapic = 0; num_bus = 0; num_ints = 0;
	int bsp_apic_id = -1, max_apic_id = -1, max_ioapic_id = -1;
	uint16_t num_entries = mpct->entry_count;
	addr_t entry_addr = (addr_t)mpct + sizeof(struct MP_CONFIGURATION_TABLE);
	for (int i = 0; i < num_entries; i++) {
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
				if (entry->u.ioapic.flags & MP_IOAPIC_FLAG_EN)
					num_ioapic++;
				if (entry->u.ioapic.ioapic_id > max_ioapic_id)
					max_ioapic_id = entry->u.ioapic.ioapic_id;
				entry_addr += 8;
				break;
			case MP_ENTRY_TYPE_BUS:
				num_bus++;
				entry_addr += 8;
				break;
			case MP_ENTRY_TYPE_IOINT:
				num_ints++;
				entry_addr += 8;
				break;
			default:
				entry_addr += 8;
				break;
		}
	}
	if (bsp_apic_id == -1)
		panic("smp: no BSP processor defined!");

	kprintf("SMP: %u CPU(s) detected, %u IOAPIC(s)\n", num_cpu, num_ioapic);

	/*
	 * Prepare the LAPIC ID -> CPU ID array, we use this to quickly look up
	 * the corresponding CPU structures.
	 */
	lapic2cpuid = kmalloc(max_apic_id);

	/* Prepare the CPU structure. CPU #0 is always the BSP */
	cpus = (struct IA32_CPU**)kmalloc((sizeof(struct IA32_CPU*) + sizeof(struct IA32_CPU)) * num_cpu);
	memset(cpus, 0, (sizeof(struct IA32_CPU*) + sizeof(struct IA32_CPU)) * num_cpu);
	for (int i = 0; i < num_cpu; i++) {
		cpus[i] = (struct IA32_CPU*)(cpus + (sizeof(struct IA32_CPU*) * num_cpu) + i * sizeof(struct IA32_CPU));

		/*
	 	 * Note that we don't need to allocate anything extra for the BSP, but
		 * we have to setup the pointers.
		 */
		if (i == 0) {
extern void* gdt;
			cpus[i]->gdt = gdt;
			continue;
		}

		/*
		 * Allocate one buffer and place all necessary administration in there.
		 * Each AP needs an own GDT because it contains the pointer to the per-CPU
		 * data and the TSS must be distinct too.
		 */
		char* buf = kmalloc(GDT_NUM_ENTRIES * 8 + sizeof(struct TSS) + sizeof(struct PCPU));
		cpus[i]->gdt = buf;
extern void* gdt;
		memcpy(cpus[i]->gdt, &gdt, GDT_NUM_ENTRIES * 8);
struct TSS* tss = (struct TSS*)(buf + GDT_NUM_ENTRIES * 8);
		memset(tss, 0, sizeof(struct TSS));
		tss->ss0 = GDT_SEL_KERNEL_DATA;
		GDT_SET_TSS(cpus[i]->gdt, GDT_IDX_KERNEL_TASK, 0, (addr_t)tss, sizeof(struct TSS));
		cpus[i]->tss = (char*)tss;

		/* Initialize per-CPU data */
		struct PCPU* pcpu = (struct PCPU*)(buf + GDT_NUM_ENTRIES * 8 + sizeof(struct TSS));
		memset(pcpu, 0, sizeof(struct PCPU));
		pcpu->cpuid = i;
		pcpu->tss = (addr_t)cpus[i]->tss;
		pcpu_init(pcpu);
		GDT_SET_ENTRY32(cpus[i]->gdt, GDT_IDX_KERNEL_PCPU, SEG_TYPE_DATA, SEG_DPL_SUPERVISOR, (addr_t)pcpu, sizeof(struct PCPU));

		/* Use the idle thread stack to execute from; we're becoming the idle thread anyway */
		cpus[i]->stack = (void*)pcpu->idlethread.md_esp;
	}

	/* Prepare the IOAPIC structure */
	ioapics = (struct IA32_IOAPIC**)kmalloc((sizeof(struct IA32_IOAPIC*) + sizeof(struct IA32_IOAPIC)) * num_ioapic);
	memset(ioapics, 0, ((sizeof(struct IA32_IOAPIC*) + sizeof(struct IA32_IOAPIC)) * num_ioapic));
	for (int i = 0; i < num_ioapic ; i++) {
		ioapics[i] = (struct IA32_IOAPIC*)(ioapics + (sizeof(struct IA32_IOAPIC*) * num_ioapic) + i * sizeof(struct IA32_IOAPIC));
	}

	/* Prepare the BUS structure */
	busses = (struct IA32_BUS**)kmalloc((sizeof(struct IA32_BUS*) + sizeof(struct IA32_BUS)) * num_bus);
	memset(busses, 0, (sizeof(struct IA32_BUS*) + sizeof(struct IA32_BUS)) * num_bus);
	for (int i = 0; i < num_bus ; i++) {
		busses[i] = (struct IA32_BUS*)(busses + (sizeof(struct IA32_BUS*) * num_bus) + i * sizeof(struct IA32_BUS));
	}

	/* Prepare the INTERRUPTS structure */
	ints = (struct IA32_INTERRUPT**)kmalloc((sizeof(struct IA32_INTERRUPT*) + sizeof(struct IA32_INTERRUPT)) * num_ints);
	memset(ints, 0, (sizeof(struct IA32_INTERRUPT*) + sizeof(struct IA32_INTERRUPT)) * num_ints);
	for (int i = 0; i < num_ints; i++) {
		ints[i] = (struct IA32_INTERRUPT*)(ints + (sizeof(struct IA32_INTERRUPT*) * num_ints) + i * sizeof(struct IA32_INTERRUPT));
	}

	/* Wade through the table */
	int cur_cpu = 0, cur_ioapic = 0, cur_bus = 0, cur_int = 0;
	int cur_ioapic_first = 0;
	entry_addr = (addr_t)mpct + sizeof(struct MP_CONFIGURATION_TABLE);
	for (int i = 0; i < num_entries; i++) {
		struct MP_ENTRY* entry = (struct MP_ENTRY*)entry_addr;
		switch(entry->type) {
			case MP_ENTRY_TYPE_PROCESSOR:
				if ((entry->u.proc.flags & MP_PROC_FLAG_EN) == 0)
					break;
				kprintf("cpu%u: %s, id #%u\n",
				 cur_cpu, bsp_apic_id == entry->u.proc.lapic_id ? "BSP" : "AP",
				 entry->u.proc.lapic_id);
				lapic2cpuid[entry->u.proc.lapic_id] = cur_cpu;
				cur_cpu++;
				break;
			case MP_ENTRY_TYPE_IOAPIC:
				if ((entry->u.ioapic.flags & MP_IOAPIC_FLAG_EN) == 0)
					break;
#ifdef SMP_DEBUG
				kprintf("ioapic%u: id #%u, mem %x\n",
				 cur_ioapic, entry->u.ioapic.ioapic_id,
				 entry->u.ioapic.addr);
#endif
				ioapics[cur_ioapic]->ioa_id = entry->u.ioapic.ioapic_id;
				ioapics[cur_ioapic]->ioa_addr = entry->u.ioapic.addr;
				/* XXX Assumes the address is in kernel space (it should be) */
				vm_map_kernel(entry->u.ioapic.addr, 1, VM_FLAG_READ | VM_FLAG_WRITE);
#ifdef notyet
				/*
				 * Is this really necessary or just over-paranoid? It fails with a 1-CPU Bochs...
				 */
				if ((ioapic_read(ioapics[cur_ioapic], IOAPICID) >> 24) & 7 != entry->u.ioapic.ioapic_id)
					panic("smp: ioapic doesn't agree with its own id");
#endif

				/* Fetch IOAPIC version register; this contains the number of interrupts supported */
				uint32_t num_ints = ((ioapic_read(ioapics[cur_ioapic], IOAPICVER) >> 16) & 0xff) + 1;

				/* Set up the IRQ source; each IOAPIC covers 24 interrupt sources */
				ioapics[cur_ioapic]->ioa_source.is_privdata = &ioapics[cur_ioapic];
				ioapics[cur_ioapic]->ioa_source.is_first = cur_ioapic_first;
				ioapics[cur_ioapic]->ioa_source.is_count = num_ints;
				ioapics[cur_ioapic]->ioa_source.is_mask = ioapic_mask;
				ioapics[cur_ioapic]->ioa_source.is_unmask = ioapic_unmask;
				ioapics[cur_ioapic]->ioa_source.is_ack = ioapic_ack;
				irqsource_register(&ioapics[cur_ioapic]->ioa_source);
				cur_ioapic++; cur_ioapic_first += num_ints;
				break;
			case MP_ENTRY_TYPE_IOINT:
#ifdef SMP_DEBUG
				kprintf("Interrupt: type %u flags %u bus %u irq %u, ioapic %u int %u \n",
				 entry->u.interrupt.type,
				 entry->u.interrupt.flags,
				 entry->u.interrupt.source_busid,
				 entry->u.interrupt.source_irq,
				 entry->u.interrupt.dest_ioapicid,
				 entry->u.interrupt.dest_ioapicint);
#endif
				ints[cur_int]->source_no = entry->u.interrupt.source_irq;
				ints[cur_int]->dest_no = entry->u.interrupt.dest_ioapicint;
				ints[cur_int]->bus = find_bus(entry->u.interrupt.source_busid);
				ints[cur_int]->ioapic = find_ioapic(entry->u.interrupt.dest_ioapicid);
				cur_int++;
				break;
			case MP_ENTRY_TYPE_BUS:
#ifdef SMP_DEBUG
				kprintf("Bus: id %u type [%c][%c][%c][%c][%c][%c]\n",
				 entry->u.bus.bus_id,
				 entry->u.bus.type[0], entry->u.bus.type[1], entry->u.bus.type[2],
				 entry->u.bus.type[3], entry->u.bus.type[4], entry->u.bus.type[5]);
#endif
				busses[cur_bus]->id = entry->u.bus.bus_id;
				busses[cur_bus]->type = resolve_bus_type(&entry->u.bus);
				cur_bus++;
				break;
		}
		entry_addr += (entry->type == MP_ENTRY_TYPE_PROCESSOR) ? 20 : 8;
	}

	vm_unmap_kernel((addr_t)mpfps_phys_ptr, mpct_numpages);

	/* Map the local APIC memory and enable it */
	vm_map_device(LAPIC_BASE, LAPIC_SIZE);
	*((uint32_t*)LAPIC_SVR) |= LAPIC_SVR_APIC_EN;

	/* Wire the APIC for Symmetric Mode */
	if (mpfps.feature2 & MP_FPS_FEAT2_IMCRP) {
		outb(IMCR_ADDRESS, IMCR_ADDR_IMCR);
		outb(IMCR_DATA, IMCR_DATA_MP);
	}

	/* Program the I/O APIC - we currently just wire all ISA interrupts */
	for (int i = 0; i < num_ints; i++) {
#ifdef SMP_DEBUG
		kprintf("int %u: source=%u, dest=%u, bus=%x, apic=%x\n",
		 i, ints[i]->source_no, ints[i]->dest_no, ints[i]->bus, ints[i]->ioapic);
#endif

		if (ints[i]->bus == NULL || ints[i]->bus->type != BUS_TYPE_ISA)
			continue;
		if (ints[i]->ioapic == NULL)
			continue;

		/* XXX For now, route all interrupts to the BSP */
		uint32_t reg = IOREDTBL + (ints[i]->dest_no * 2);
		ioapic_write(ints[i]->ioapic, reg, TRIGGER_EDGE | DESTMOD_PHYSICAL | DELMOD_FIXED | (ints[i]->source_no + 0x20));
		ioapic_write(ints[i]->ioapic, reg + 1, bsp_apic_id << 24);
	}

	/*
	 * Register an interrupt source for the IPI's; they appear as normal
 	 * interrupts and this lets us process them as such.
	 */
	irqsource_register(&ipi_source);
	if (irq_register(SMP_IPI_PANIC, NULL, smp_ipi_panic) != ANANAS_ERROR_OK)
		panic("can't register ipi");
	if (irq_register(SMP_IPI_SCHEDULE, NULL, smp_ipi_schedule) != ANANAS_ERROR_OK)
		panic("can't register ipi");

	/*
	 * Initialize the SMP launch variable; every AP will just spin and check this value. We don't
	 * care about races here, because it doesn't matter if an AP needs a few moments to determine
	 * that it needs to run.
	 */
	can_smp_launch = 0;

	return ANANAS_ERROR_OK;

smp_abort:
	md_remove_low_mappings();
	return ANANAS_ERROR(NO_DEVICE);
}

/*
 * Called on the Boot Strap Processor, in order to fully launch the AP's.
 */
static errorcode_t
smp_launch()
{
	can_smp_launch++;

	/*
	 * Broadcast INIT-SIPI-SIPI-IPI to all AP's; this will wake them up and cause
	 * them to run the AP entry code.
	 */
	*((uint32_t*)LAPIC_ICR_LO) = LAPIC_ICR_DEST_ALL_EXC_SELF | LAPIC_ICR_LEVEL_ASSERT | LAPIC_ICR_DELIVERY_INIT;
	delay(10);
	*((uint32_t*)LAPIC_ICR_LO) = LAPIC_ICR_DEST_ALL_EXC_SELF | LAPIC_ICR_LEVEL_ASSERT | LAPIC_ICR_DELIVERY_SIPI | (addr_t)KVTOP((addr_t)ap_code) >> 12;
	delay(200);
	*((uint32_t*)LAPIC_ICR_LO) = LAPIC_ICR_DEST_ALL_EXC_SELF | LAPIC_ICR_LEVEL_ASSERT | LAPIC_ICR_DELIVERY_SIPI | (addr_t)KVTOP((addr_t)ap_code) >> 12;
	delay(200);

	while(num_smp_launched < num_cpu)
		/* wait for it ... */ ;

	/* All done - we can throw away the AP code and mappings */
	kfree(ap_code);
	md_remove_low_mappings();

	return ANANAS_ERROR_OK;
}

INIT_FUNCTION(smp_launch, SUBSYSTEM_SCHEDULER, ORDER_MIDDLE);

void
smp_panic_others()
{
	*((uint32_t*)LAPIC_ICR_LO) = LAPIC_ICR_DEST_ALL_EXC_SELF | LAPIC_ICR_LEVEL_ASSERT | LAPIC_ICR_DELIVERY_FIXED | SMP_IPI_PANIC;
}

void
smp_broadcast_schedule()
{
	*((uint32_t*)LAPIC_ICR_LO) = LAPIC_ICR_DEST_ALL_INC_SELF | LAPIC_ICR_LEVEL_ASSERT | LAPIC_ICR_DELIVERY_FIXED | SMP_IPI_SCHEDULE;
}

/*
 * Called by mp_stub.S for every Application Processor. Should not return.
 */
void
mp_ap_startup(uint32_t lapic_id)
{
	/* Switch to our idle thread */
	thread_t* idlethread = PCPU_GET(idlethread_ptr);
  PCPU_SET(curthread, idlethread);
  scheduler_add_thread(idlethread);

	/* Enable the LAPIC for this AP so we can handle interrupts */
	*((uint32_t*)LAPIC_SVR) |= LAPIC_SVR_APIC_EN;

	/* Wait for it ... */
	while (!can_smp_launch)
		/* nothing */ ;

	/* We're up and running! Increment the launched count */
	__asm("lock incl (num_smp_launched)");
	
	/* Enable interrupts and become the idle thread; this doesn't return */
	md_interrupts_enable();
	idle_thread();
}

/* vim:set ts=2 sw=2: */
