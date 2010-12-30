#include <ananas/x86/io.h>
#include <ananas/x86/pit.h>
#include <ananas/pcpu.h>
#include <ananas/lib.h>

#define TIMER_FREQ 1193182

#define HZ 100 /* XXX does not belong here */

void
x86_pit_init()
{
	uint16_t count = TIMER_FREQ / HZ;
	outb(PIT_MODE_CMD, PIT_CH_CHAN0 | PIT_MODE_3 | PIT_ACCESS_BOTH);
	outb(PIT_CH0_DATA, (count & 0xff));
	outb(PIT_CH0_DATA, (count >> 8));
}
	
/* vim:set ts=2 sw=2: */
