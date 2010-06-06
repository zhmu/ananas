#include <sys/lib.h> 
#include <ofw.h> 
#include <stdio.h> /* for printf */

#define OFW_HEAP_SIZE 0x8000

static ofw_cell_t ofw_ihandle_stdin  = 0;
static ofw_cell_t ofw_ihandle_stdout = 0;
static void* ofw_heap_base = NULL;
static unsigned int ofw_heap_pos = 0;
static uint32_t ofw_memory_size = 0;

void*
ofw_heap_alloc(unsigned int size)
{
	if (ofw_heap_pos + size > OFW_HEAP_SIZE)
		printf("FATAL: out of heap space!\n");
	void* ptr = (void*)(ofw_heap_base + ofw_heap_pos);
	ofw_heap_pos += size;
	return ptr;
}

unsigned int
ofw_heap_left()
{
	return OFW_HEAP_SIZE - ofw_heap_pos;
}

uint32_t
ofw_get_memory_size()
{
	return ofw_memory_size;
}

void
ofw_putch(char ch)
{
	if (ch == '\n') {
		ofw_write(ofw_ihandle_stdout, &ch, 1);
		ch = '\r';
	}
	ofw_write(ofw_ihandle_stdout, &ch, 1);
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

	/* Get the first block of available data; this will be our scratchpad */
	struct ofw_reg avail;
	ofw_getprop(p_memory, "available", &avail, sizeof(avail));
	ofw_heap_base = ofw_claim(avail.base, OFW_HEAP_SIZE);
	if (ofw_heap_base == (void*)-1) {
		printf("FATAL: cannot establish heap!\n");
		return;
	}

	/*
	 * Now, we should calculate the complete memory size; we only use it for
	 * informative purposes (but it sure looks nice :-)
	 */
	struct ofw_reg* reg = ofw_heap_alloc(0);
	unsigned int reglen = ofw_getprop(p_memory, "reg", reg, ofw_heap_left());
	while (reglen > 0) {
		ofw_memory_size += reg->size >> 10;
		reg++; reglen -= sizeof(struct ofw_reg);
	}
}

void
ofw_cleanup()
{
	if (ofw_heap_base != NULL)
		ofw_release((ofw_cell_t)ofw_heap_base, OFW_HEAP_SIZE);
}

/* vim:set ts=2 sw=2: */
