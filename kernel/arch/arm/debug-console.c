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

#define BOARD_VERSATILEPB
//#define BOARD_SHEEVAPLUG

#ifdef BOARD_VERSATILEPB
# define UART0_BASE 0x101f1000
#elif defined(BOARD_SHEEVAPLUG)
# define UART0_BASE MV_UART_BASE
#else
# error Unknown platform
#endif

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
#ifdef BOARD_SHEEVAPLUG
	while ((*(volatile uint32_t*)MV_UART_LSR(KERNEL_UART0) & MV_UART_LSR_THRE) == 0);
#endif
	*(volatile uint32_t*)KERNEL_UART0 = ch;
}

int
debugcon_getch()
{
	return 0;
}

/* vim:set ts=2 sw=2: */
