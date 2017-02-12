#include <ananas/lib.h> 
#include <ananas/mm.h>
#include <machine/param.h> 
#include <ofw.h> 

static ofw_cell_t ofw_ihandle_stdin  = 0;
static ofw_cell_t ofw_ihandle_stdout = 0;

void
ofw_putch(char ch)
{
	if (ch == '\n') {
		ofw_write(ofw_ihandle_stdout, &ch, 1);
		ch = '\r';
	}
	ofw_write(ofw_ihandle_stdout, &ch, 1);
}

int
ofw_getch()
{
	unsigned char ch;

	while (ofw_read(ofw_ihandle_stdin, &ch, 1) == 0)
		/* wait */;
	return ch;
}

void
ofw_init_io()
{
	/* Obtain the in/output device handles; once we have them, we can putch/getch */
	ofw_cell_t chosen = ofw_finddevice("/chosen");
	ofw_getprop(chosen, "stdin",  &ofw_ihandle_stdin,  sizeof(ofw_ihandle_stdin));
	ofw_getprop(chosen, "stdout", &ofw_ihandle_stdout, sizeof(ofw_ihandle_stdout));
}

/* vim:set ts=2 sw=2: */
