#ifndef __ANANAS_MV_REGS_H__
#define __ANANAS_MV_REGS_H__

/*
 * Marvell registers as they appear on the SheevaPlug.
 */

#define REG_BASE	0xf1000000
#define UART_BASE (REG_BASE+0x12000)

/* Register names are from 88F6281 Function Specification, p 678 */
#define UART0_THR		(UART_BASE+0x0000)			/* Transmit Holding Register */
#define UART0_DLL		(UART_BASE+0x0000)			/* Divisor Latch Low register */
#define UART0_RBR		(UART_BASE+0x0000)			/* Receive Buffer Register */
#define UART0_IER		(UART_BASE+0x0004)			/* Interrupt Enable Register */
#define UART0_DLH		(UART_BASE+0x0004)			/* Divisor Latch High register */
#define UART0_IIR		(UART_BASE+0x0008)			/* Interrupt Identify Register */
#define UART0_FCR		(UART_BASE+0x0008)			/* FIFO Control Register */
#define UART0_LCR		(UART_BASE+0x000c)			/* Line Control Register */
#define UART0_MCR		(UART_BASE+0x0010)			/* Modem Control Register */
#define UART0_LSR		(UART_BASE+0x0014)			/* Line Status Register */
#define UART0_LSR_DATARXSTAT		(1 << 0)			/* Receive buffer contains char */
#define UART0_LSR_OVERRUNERR		(1 << 1)			/* Overrun error */
#define UART0_LSR_PARERR		(1 << 2)			/* Parity error */
#define UART0_LSR_FRAMEERR		(1 << 3)			/* Frame error */
#define UART0_LSR_BI			(1 << 4)			/* Serial input held lo too long */
#define UART0_LSR_THRE			(1 << 5)			/* Transmitter Holding Register Empty */
#define UART0_LSR_TXEMPTY		(1 << 6)			/* Transmitter Holding TX Empty */
#define UART0_LSR_RXFIFOERR		(1 << 7)			/* FIFO parity/framing error */
#define UART0_MSR		(UART_BASE+0x0018)			/* Modem Status Register */
#define UART0_SCR		(UART_BASE+0x001c)			/* Scratch Pad Register */

#endif /* __ANANAS_MV_REGS_H__ */
