#include <ananas/lib.h>

void
exc_reset()
{
	panic("Reset exc");
}

void
exc_undef()
{
	panic("Undefined instruction");
}

void
exc_swi()
{
	panic("SWI");
}

void
exc_pre_abort()
{
	panic("Pre-abort exc");
}

void
exc_data_abort()
{
	panic("Data abort");
}

void
exc_irq()
{
	panic("IRQ");
}

void
exc_fiq()
{
	panic("FIQ");
}

/* vim:set ts=2 sw=2: */
