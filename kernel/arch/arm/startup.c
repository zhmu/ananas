#include <ananas/types.h>
#include <ananas/arm/mv-regs.h>
#include <ananas/arm/param.h>
#include <ananas/arm/vm.h>
#include <ananas/pcpu.h>
#include <ananas/lib.h>

struct PCPU cpu_pcpu;

/* Bootinfo as supplied by the loader, or NULL */
struct BOOTINFO* bootinfo = NULL;
//static struct BOOTINFO _bootinfo;
extern void *__entry, *__end;

static void
putcr(int c)
{
	while ((*(uint32_t*)UART0_LSR & UART0_LSR_THRE) == 0);
	*(uint32_t*)UART0_RBR = c;
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
	uint32_t* l2 = (uint32_t*)((l1val & 0xfffff000) + (((va >> 12) & 0xff) * 4));
	uint32_t  l2val = *(uint32_t*)l2;

	putcr('v'); putcr('a'); putcr('='); /* va= */
	puthex(va);
	putcr(' '); putcr('&'); putcr('l'); putcr('1'); putcr('='); /* &l1= */
	puthex((uint32_t)l1);
	putcr(' '); putcr('*'); putcr('l'); putcr('1'); putcr('='); /* *l1= */
	puthex(l1val);
	putcr(' '); putcr('&'); putcr('l'); putcr('2'); putcr('='); /* &l2= */
	puthex((uint32_t)l2);
	putcr(' '); putcr('*'); putcr('l'); putcr('2'); putcr('='); /* *l2= */
	puthex(l2val);
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
	/*
	 * These are our first few steps in the world of boot - first of all, we
	 * need to get the memory mappings right - this code is running from
	 * unmapped memory addresses and there is no MMU enabled.
	 *
	 * Prepare pagetables; we need to map the lower kernel addresses (where
	 * we are now) and the upper kernel addresses (where we want to execute
	 * from) - note that we must map the initial table to TT_SIZE-sized boundary
	 * to avoid problems.
	 */
	addr_t address = (addr_t)&__end - KERNBASE;

	/* Create the translation table; i.e. the page directory */
	uint32_t* tt = (uint32_t*)address;
	memset(tt, 0, VM_TT_SIZE);
	address += VM_TT_SIZE;

	putcr('t'); putcr('t'); putcr('=');
	puthex((uint32_t)tt);
	putcr('\r'); putcr('\n');
	KASSERT(((addr_t)tt % VM_TT_SIZE) == 0, "tt not aligned");

	/* Create a pointer to the second-level structures; these are 1kB a piece */
	addr_t l2_ptr = address;
	memset((void*)l2_ptr, 0, VM_L2_TOTAL_SIZE);
	address += VM_L2_TOTAL_SIZE;

	/* Hook up the initial kernel second-level structure mappings */
	for (int i = 0; i < VM_TT_KERNEL_NUM; i++) {
		tt[VM_TT_KERNEL_START + i] = l2_ptr | 2 /* XXX type, domain=0 */;
		l2_ptr += VM_L2_TABLE_SIZE;
	}

	/* Create the kernel table entry mappings */
	for (addr_t n = (addr_t)&__entry; n < (addr_t)&__end; n += PAGE_SIZE) {
	//for (addr_t n = (addr_t)&__entry; n <= (addr_t)&__entry; n += PAGE_SIZE) {
		uint32_t* l1 = (uint32_t*)&tt[n >> 20]; /* note that typeof(tt) is uint32_t, so the 00 bits are added */
		uint32_t* l2 = (uint32_t*)((*l1 & 0xfffff000) + (((n >> 12) & 0xff) * 4));
		*l2 = ((n - KERNBASE) & 0xfffff000) | (2 << 4) /* XXX super r/w user ro */ | 3 /* XXX type */ ;

#if 0
		putcr('&'); putcr('l'); putcr('1'); putcr('='); /* &l1= */
		puthex((uint32_t)l1);
		putcr(' '); putcr('*'); putcr('l'); putcr('1'); putcr('='); /* *l1= */
		puthex(*(uint32_t*)l1);

		putcr(' '); putcr('&'); putcr('l'); putcr('2'); putcr('='); /* &l2= */
		puthex((uint32_t)l2);

		putcr(' '); putcr('-'); putcr('>'); putcr(' '); /* -> */
		putcr('*'); putcr('l'); putcr('2'); putcr('='); /* *l2= */
		puthex(*(uint32_t*)l2);
		putcr('\r'); putcr('\n');
#endif
	}

#if 0
	/* map UART */
	do {
		uint32_t n = UART_BASE;
		uint32_t* l1 = (uint32_t*)&tt[n >> 20]; /* note that typeof(tt) is uint32_t, so the 00 bits are added */
		uint32_t* l2 = (uint32_t*)((*l1 & 0xfffff000) + (((n >> 12) & 0xff) * 4));
		//*l2 = (n & 0xfffff000) | (2 << 6) /* XXX non-share device */ | (2 << 4) /* XXX super r/w user ro */ | 4 /* device */ | 3 /* XXX type */ ;
		*l2 = ((n - KERNBASE) & 0xfffff000) | (2 << 4) /* XXX super r/w user ro */ | 4 /* device */ | 3 /* XXX type */ ;
	} while(0);
#endif

	/*
	 * Establish an identity mapping for the current kernel addresses; this is necessary
	 * so that we can make the switch to MMU-world without losing our current code.
	 */
	for (addr_t n = (addr_t)&__entry - KERNBASE; n < (addr_t)&__end - KERNBASE; n += PAGE_SIZE) {
		tt[n >> 20] = tt[(n | KERNBASE) >> 20];
	}

	resolve(tt, 0x900000);
	resolve(tt, (uint32_t)&__entry);
	resolve(tt, 0x900000 + PAGE_SIZE);
	resolve(tt, (uint32_t)&__entry + PAGE_SIZE);
	resolve(tt, 0xf1012014);

	/* Time to throw... the switch: the pagetables need to be set */
	__asm __volatile(
		"mcr p15, 0, %0, c2, c0, 0\n"		/* set translation table base */
		"mcr p15, 0, %1, c8, c7, 0\n"		/* invalidate entire TLB */
		"mov r1, #1\n"									/* domain stuff */
		"mcr p15, 0, r1, c3, c0, 0\n"
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

	/* Throw away the identity mappings */
	for (addr_t n = (addr_t)&__entry - KERNBASE; n < (addr_t)&__end - KERNBASE; n += PAGE_SIZE) {
		tt[n >> 20] = 0;
	}
	
#if 0
	putcr('H');
	putcr('e');
	putcr('l');
	putcr('l');
	putcr('o');
	puthex((addr_t)&__entry);
#endif
	for(;;) {
		foo();
	}
}

/* vim:set ts=2 sw=2: */
