#ifndef __ANANAS_WII_PI_H__
#define __ANANAS_WII_PI_H__

#define PI_INTCAUSE 0x0
#define  PI_INT_ERROR		(1 << 0)		/* GP Runtime Error */
#define  PI_INT_RESET		(1 << 1)		/* Reset Switch */
#define  PI_INT_DI		(1 << 2)		/* DVD drive */
#define  PI_INT_SI		(1 << 3)		/* Serial interface */
#define  PI_INT_EXI		(1 << 4)		/* EXI */
#define  PI_INT_AI		(1 << 5)		/* Audio Interface */
#define  PI_INT_DSP		(1 << 6)		/* DSP */
#define  PI_INT_MEM		(1 << 7)		/* Memory Interface */
#define  PI_INT_VI		(1 << 8)		/* Video Interface */
#define  PI_INT_PETOKEN		(1 << 9)		/* PE Token */
#define  PI_INT_PEFINISH	(1 << 10)		/* PE Finish */
#define  PI_INT_CP		(1 << 11)		/* Command Processor FIFO */
#define  PI_INT_DEBUG		(1 << 12)		/* Debugger */
#define  PI_INT_HSP		(1 << 13)		/* High Speed Port */
#define  PI_INT_HOLLYWOOD	(1 << 14)		/* Hollywood IRQs */
#define  PI_INT_RESETSTATE	(1 << 16)		/* Reset Switch State */
#define  PI_INT_MASK		((1 << 17) - 1)

#define PI_INTMASK 0x4

#endif /* __ANANAS_WII_PI_H__*/
