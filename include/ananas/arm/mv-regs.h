#ifndef __ANANAS_MV_REGS_H__
#define __ANANAS_MV_REGS_H__

/*
 * Marvell registers as they appear on the SheevaPlug.
 */

#define MV_REG_BASE	0xf1000000
#define MV_UART_BASE	(MV_REG_BASE+0x12000)

/* Register names are from 88F6281 Function Specification, p 678 */
#define MV_UART_THR(x)		((x)+0x0000)			/* Transmit Holding Register */
#define MV_UART_DLL(x)		((x)+0x0000)			/* Divisor Latch Low register */
#define MV_UART_RBR(x)		((x)+0x0000)			/* Receive Buffer Register */
#define MV_UART_IER(x)		((x)+0x0004)			/* Interrupt Enable Register */
#define MV_UART_DLH(x)		((x)+0x0004)			/* Divisor Latch High register */
#define MV_UART_IIR(x)		((x)+0x0008)			/* Interrupt Identify Register */
#define MV_UART_FCR(x)		((x)+0x0008)			/* FIFO Control Register */
#define MV_UART_LCR(x)		((x)+0x000c)			/* Line Control Register */
#define MV_UART_MCR(x)		((x)+0x0010)			/* Modem Control Register */
#define MV_UART_LSR(x)		((x)+0x0014)			/* Line Status Register */
#define MV_UART_LSR_DATARXSTAT		(1 << 0)			/* Receive buffer contains char */
#define MV_UART_LSR_OVERRUNERR		(1 << 1)			/* Overrun error */
#define MV_UART_LSR_PARERR		(1 << 2)			/* Parity error */
#define MV_UART_LSR_FRAMEERR		(1 << 3)			/* Frame error */
#define MV_UART_LSR_BI			(1 << 4)			/* Serial input held lo too long */
#define MV_UART_LSR_THRE			(1 << 5)			/* Transmitter Holding Register Empty */
#define MV_UART_LSR_TXEMPTY		(1 << 6)			/* Transmitter Holding TX Empty */
#define MV_UART_LSR_RXFIFOERR		(1 << 7)			/* FIFO parity/framing error */
#define MV_UART_MSR(x)		((x)+0x0018)			/* Modem Status Register */
#define MV_UART_SCR(x)		((x)+0x001c)			/* Scratch Pad Register */

#endif /* __ANANAS_MV_REGS_H__ */
