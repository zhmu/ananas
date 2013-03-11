#include <ananas/types.h>
#include <machine/mv-regs.h>
#include <machine/vm.h>
#include <ananas/console.h>
#include <ananas/debug-console.h>
#include <ananas/vm.h>

/*
 * UART is mapped to this address; must be above KERNBASE because we can't yet
 * allocate a new page for L2 entries.
 */
#define KERNEL_UART0 0xfffe1000

#if 1
/* XXX For VersatilePB board */
#define UART0_BASE 0x101f1000
#else
#define UART0_BASE UART_BASE
#endif
#define UART0_DR (UART0_BASE)

void
debugcon_init()
{
	/*
	 * Map our UART to a kernel-space address; we know the pages there are
	 * backed so we do not need to allocate extra mappings.
	 */
	md_mapto(vm_kernel_tt, KERNEL_UART0, UART0_BASE, 1, VM_FLAG_DEVICE | VM_FLAG_READ | VM_FLAG_WRITE);
}

void
debugcon_putch(int ch)
{
	*(volatile uint32_t*)KERNEL_UART0 = ch;
}

int
debugcon_getch()
{
	return 0;
}

/* vim:set ts=2 sw=2: */
