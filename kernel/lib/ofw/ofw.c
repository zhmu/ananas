#include <sys/lib.h> 
#include <sys/mm.h>
#include <machine/param.h> 
#include <ofw.h> 

#define OFW_HEAP_SIZE 0x8000

static ofw_cell_t ofw_ihandle_stdin  = 0;
static ofw_cell_t ofw_ihandle_stdout = 0;
static void* ofw_heap_base = NULL;
static unsigned int ofw_heap_pos = 0;
static uint32_t ofw_memory_size = 0;

#define OFW_MAX_AVAIL 32
static struct ofw_reg ofw_avail[OFW_MAX_AVAIL];

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
ofw_init()
{
	/* Obtain the in/output device handles; once we have them, we can putch/getch */
	ofw_cell_t chosen = ofw_finddevice("/chosen");
	ofw_getprop(chosen, "stdin",  &ofw_ihandle_stdin,  sizeof(ofw_ihandle_stdin));
	ofw_getprop(chosen, "stdout", &ofw_ihandle_stdout, sizeof(ofw_ihandle_stdout));

	/* Fetch the memory phandle; we need this to obtain memory we can use */
	ofw_cell_t i_memory;
	ofw_getprop(chosen, "memory", &i_memory,           sizeof(i_memory));
	ofw_cell_t p_memory = ofw_instance_to_package(i_memory);

	/* Bootstrap the memory manager */
	unsigned int availlen = ofw_getprop(p_memory, "available", ofw_avail, sizeof(struct ofw_reg) * OFW_MAX_AVAIL);
	for (unsigned int i = 0; i < availlen / sizeof(struct ofw_reg); i++) {
		uint32_t base = (ofw_avail[i].base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
		uint32_t len  = ofw_avail[i].size & ~(PAGE_SIZE - 1);
		mm_zone_add(base, len);
	}

	mi_startup();
}

/* vim:set ts=2 sw=2: */
