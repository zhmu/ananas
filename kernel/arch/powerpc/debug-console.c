#include <ananas/types.h>
#include <ananas/debug-console.h>
#include <ofw.h>

void
debugcon_init()
{
}

void
debugcon_putch(int ch)
{
	ofw_putch(ch);
}

int
debugcon_getch()
{
	return 0;
}

/* vim:set ts=2 sw=2: */
