#ifndef __OPENFIRMWARE_H__
#define __OPENFIRMWARE_H__

/* 6.3.1 calls a field in the OFW structure 'cel'l */
typedef unsigned long int ofw_cell_t;

/* Entry point to OFW services */
typedef int (*ofw_entry_t)(void*);
extern ofw_entry_t ofw_entry;

/* 'reg' structure */
struct ofw_reg {
	ofw_cell_t	base;
	ofw_cell_t	size;
};

/* general functions */
void ofw_init();
void ofw_putch(char ch);
void* ofw_heap_alloc(unsigned int size);
uint32_t ofw_get_memory_size();

/* device i/o */
int ofw_read(ofw_cell_t ihandle, void* buf, unsigned int length);
int ofw_write(ofw_cell_t ihandle, const void* buf, unsigned int length);
int ofw_seek(ofw_cell_t ihandle, uint64_t position);

/* device tree */
ofw_cell_t ofw_finddevice(const char* device);
int ofw_getproplen(ofw_cell_t phandle, const char* name);
int ofw_getprop(ofw_cell_t phandle, const char* name, void* buf, int length);
ofw_cell_t ofw_instance_to_package(ofw_cell_t ihandle);

/* memory */
void* ofw_claim(ofw_cell_t virt, ofw_cell_t size);
void ofw_release(ofw_cell_t virt, ofw_cell_t size);

#endif /* __OPENFIRMWARE_H__ */
