#include <ananas/types.h>
//#include <ananas/arm/mv-regs.h>
#include <ananas/arm/param.h>
#include <ananas/arm/vm.h>
#include <ananas/pcpu.h>
#include <ananas/lib.h>

struct PCPU cpu_pcpu;

/* Bootinfo as supplied by the loader, or NULL */
struct BOOTINFO* bootinfo = NULL;
//static struct BOOTINFO _bootinfo;
extern void *__entry, *__end;

/*
 * UART is mapped to this address; must be above KERNBASE because we assume it
 * can be mapped without having to assign a new L2 entry.
 */
#define KERNEL_UART0 0xf0001000

/* XXX For VersatilePB board */
#define UART0_BASE 0x101f1000
#define UART0_DR (UART0_BASE)

void
v_reset()
{
	while(1);
}

void
v_undef()
{
	while(1);
}

void
v_swi()
{
	while(1);
}

void
v_pre_abort()
{
	while(1);
}

void
v_data_abort()
{
	while(1);
}

void
v_irq()
{
	while(1);
}

void
v_fiq()
{
	while(1);
}

static void
putcr(int c)
{
	//while ((*(uint32_t*)UART0_LSR & UART0_LSR_THRE) == 0);
	//*(uint32_t*)UART0_RBR = c;
	*(volatile uint32_t*)KERNEL_UART0 = c;
}

static void putn(int n)
{
	if (n >= 10 && n <= 16)
		putcr('a' + (n - 10));
	else if (n >= 0 && n < 10)
		putcr('0' + n);
	else
		putcr('?');
}

static void putbyte(int x)
{
	putn(x >> 4); putn(x & 15);
}

static void puthex(uint32_t x)
{
	putbyte(x >> 24); putbyte((x >> 16) & 0xff);
	putbyte((x >> 8 ) & 0xff);
	putbyte(x & 0xff);
}

static void
resolve(uint32_t* tt, uint32_t va)
{
	uint32_t* l1 = (uint32_t*)&tt[va >> 20];
	uint32_t  l1val = *(uint32_t*)l1;
	uint32_t* l2 = (uint32_t*)((l1val & 0xfffffc00) + (((va >> 12) & 0xff) << 2));
	uint32_t  l2val = *(uint32_t*)l2;

	putcr('v'); putcr('a'); putcr('='); /* va= */
	puthex(va);
	putcr(' '); putcr('&'); putcr('l'); putcr('1'); putcr('='); /* &l1= */
	puthex((uint32_t)l1);
	putcr(' '); putcr('*'); putcr('l'); putcr('1'); putcr('='); /* *l1= */
	puthex(l1val);
	if ((l1val & 3) != 1) {
		putcr('!'); putcr('P'); putcr('\n');
		return;
	}
	putcr(' '); putcr('&'); putcr('l'); putcr('2'); putcr('='); /* &l2= */
	puthex((uint32_t)l2);
	putcr(' '); putcr('*'); putcr('l'); putcr('2'); putcr('='); /* *l2= */
	puthex(l2val);
	if ((l2val & 3) != 3) {
		putcr('!'); putcr('P');
	}
	putcr('\r'); putcr('\n');
}

static void
foo()
{	
	for (int i = 0; i < 100; i++) ;
}

void
md_startup()
{
	/* XXX Kick vectors in place */
	extern void* _vectors_begin;
	extern void* _vectors_end;
	void* dest = 0;
	memcpy(dest, (void*)((addr_t)&_vectors_begin - KERNBASE), (addr_t)&_vectors_end - (addr_t)&_vectors_begin);

	/*
	 * These are our first few steps in the world of boot - first of all, we
	 * need to get the memory mappings right - this code is running from
	 * unmapped memory addresses and there is no MMU enabled.
	 *
	 * Prepare pagetables; we need to map the lower kernel addresses (where
	 * we are now) and the upper kernel addresses (where we want to execute
	 * from) - note that we must map the initial table to TT_ALIGN-sized boundary
	 * to avoid problems.
	 *
	 * We reserve memory so that kernel memory can always be mapped without
	 * needing to allocate the coarse entry.
	 */
	addr_t address = (addr_t)&__end - KERNBASE;
	address = (address | (VM_TT_ALIGN - 1)) + 1;

	/* Create the translation table; i.e. the page directory */
	uint32_t* tt = (uint32_t*)address;
	memset(tt, 0, VM_TT_SIZE);
	address += VM_TT_SIZE;

	/* Hook up the initial kernel second-level structure mappings */
	for (int i = 0; i < VM_TT_KERNEL_NUM; i++) {
		tt[VM_TT_KERNEL_START + i] = address | VM_TT_TYPE_COARSE; /* XXX domain=0 */
		memset((void*)address, 0, VM_L2_TABLE_SIZE);
		address += VM_L2_TABLE_SIZE;
	}

	/*
	 * Create the kernel table entry mappings - we map the page tables themselves too
	 * so that we can always create kernel memory mappings.
	 */
	for (addr_t n = (addr_t)&__entry; n < (addr_t)(address + KERNBASE); n += PAGE_SIZE) {
		uint32_t* l1 = (uint32_t*)&tt[n >> 20]; /* note that typeof(tt) is uint32_t, so no << 2 is needed */
		uint32_t* l2 = (uint32_t*)((*l1 & 0xfffffc00) + (((n & 0xff000) >> 12) << 2));
		*l2 = ((n - KERNBASE) & 0xfffff000) | VM_COARSE_SMALL_AP(VM_AP_KRW_URO) | VM_COARSE_TYPE_SMALL;
	}

	/*
	 * Establish an identity mapping for the current kernel addresses; this is necessary
	 * so that we can make the switch to MMU-world without losing our current code.
	 */
	for (addr_t n = (addr_t)&__entry - KERNBASE; n < (addr_t)&__end - KERNBASE; n += PAGE_SIZE) {
		tt[n >> 20] = tt[(n | KERNBASE) >> 20];
	}

	/* Time to throw... the switch: the pagetables need to be set */
	__asm __volatile(
		"mcr p15, 0, %0, c2, c0, 0\n"		/* set translation table base */
		"mcr p15, 0, %1, c8, c7, 0\n"		/* invalidate entire TLB */
		"mov r1, #1\n"									/* domain stuff */
		"mcr p15, 0, r1, c3, c0, 0\n"

		"mov r1, #0\n"									/* control reg */
		"mcr p15, 0, r1, c2, c0, 2\n"

		"mrc p15, 0, r1, c1, c0, 0\n"
		"orr r1, r1, #1\n"							/* enable MMU */
		"mcr p15, 0, r1, c1, c0, 0\n"
		/* flush instruction cache */
		"nop\n"
		"nop\n"
		"nop\n"
		/* perform a dummy r16 read */
		"mrc p15, 0, r1, c2, c0, 0\n"
		"mov r1, r1\n"
		/* alter our instruction and stack pointer */
		"add	sp, sp, %2\n"
		"add	pc, pc, %2\n"
		"nop\n"
	: : "r" (tt), "r" (0), "r" (KERNBASE) : "r1");

	/*
	 * Map our UART to a kernel-space address; we know the pages there are
	 * backed so we do not need to allocate extra mappings.
	 */
	do {
		uint32_t va = KERNEL_UART0;
		uint32_t pa = UART0_BASE;
		uint32_t* l1 = (uint32_t*)&tt[va >> 20]; /* note that typeof(tt) is uint32_t, so the 00 bits are added */
		uint32_t* l2 = (uint32_t*)((*l1 & 0xfffffc00) + (((va & 0xff000) >> 12) << 2));
		*l2 = (pa & 0xfffff000) | VM_COARSE_SMALL_AP(VM_AP_KRW_URO) | VM_COARSE_DEVICE | VM_COARSE_TYPE_SMALL;
	} while(0);

	/* Throw away the identity mappings; these are not needed anymore */
	for (addr_t n = (addr_t)&__entry - KERNBASE; n < (addr_t)&__end - KERNBASE; n += PAGE_SIZE) {
		tt[n >> 20] = 0;
	}

#if 0
	/* Hook up the first 4KB so that we can handle exceptions */
	tt[0] = l2_ptr | 1 /* XXX type, domain=0 */;
	l2_ptr += VM_L2_TABLE_SIZE;
#endif
	
	putcr('H');
	putcr('e');
	putcr('l');
	putcr('l');
	putcr('o');
	for(;;) {
		foo();
	}

	resolve(tt, 0);
}

/* vim:set ts=2 sw=2: */
