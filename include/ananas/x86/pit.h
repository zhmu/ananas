#ifndef __X86_PIT_H__
#define __X86_PIT_H__

#define PIT_CH0_DATA 0x40		/* Channel 0 data port */
#define PIT_CH1_DATA 0x41		/* Channel 1 data port */
#define PIT_CH2_DATA 0x42		/* Channel 2 data port */
#define PIT_MODE_CMD 0x43		/* Mode/command register */
# define PIT_CH_CHAN0		(0 << 6)	/* Channel 0 */
# define PIT_CH_CHAN1		(1 << 6)	/* Channel 1 */
# define PIT_CH_CHAN2		(2 << 6)	/* Channel 2 */
# define PIT_ACCESS_LATCH	(0 << 4)	/* Latch count value command */
# define PIT_ACCESS_LOBYTE	(1 << 4)	/* Lobyte only */
# define PIT_ACCESS_HIBYTE	(2 << 4)	/* Hibyte only */
# define PIT_ACCESS_BOTH	(3 << 4)	/* Lo/hibyte */
# define PIT_MODE_0		(0 << 1)	/* Mode 0: interrupt on terminal count */
# define PIT_MODE_1		(1 << 1)	/* Mode 1: hardware retriggerable one-shot */
# define PIT_MODE_2		(2 << 1)	/* Mode 2: rate generator */
# define PIT_MODE_3		(3 << 1)	/* Mode 3: square wave generator */
# define PIT_MODE_4		(4 << 1)	/* Mode 4: software triggered strobe */
# define PIT_MODE_5		(5 << 1)	/* Mode 5: hardware triggered strobe */
# define PIT_MODE_BCD		(1 << 0)	/* BCD mode */

void x86_pit_init();
uint32_t x86_pit_calc_cpuspeed_mhz();

#endif /* __X86_PIT_H__ */
