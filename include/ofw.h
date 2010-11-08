#ifndef __OPENFIRMWARE_H__
#define __OPENFIRMWARE_H__

/* 6.3.1 calls a field in the OFW structure a 'cell' */
typedef uint32_t ofw_cell_t;

/* Entry point to OFW services */
typedef int (*ofw_entry_t)(void*);
extern int ofw_call(void*);

/* 'reg' structure */
struct ofw_reg {
	ofw_cell_t	base;
	ofw_cell_t	size;
};

/* 'translation' structure' */
struct ofw_translation {
	ofw_cell_t	virt;
	ofw_cell_t	size;	
	ofw_cell_t	phys;
	ofw_cell_t	mode;
} __attribute__((packed));

/* machine-dependant functions */
void ofw_md_init(register_t entry);
void ofw_md_setup_mmu();

/* general functions */
void ofw_init_io();
void ofw_putch(char ch);
int ofw_getch();
void* ofw_heap_alloc(unsigned int size);
uint32_t ofw_get_memory_size();

/* device i/o */
ofw_cell_t ofw_open(const char* device);
void ofw_close(ofw_cell_t ihandle);
int ofw_read(ofw_cell_t ihandle, void* buf, unsigned int length);
int ofw_write(ofw_cell_t ihandle, const void* buf, unsigned int length);
int ofw_seek(ofw_cell_t ihandle, uint64_t position);

/* device tree */
ofw_cell_t ofw_peer(ofw_cell_t phandle);
ofw_cell_t ofw_child(ofw_cell_t phandle);
ofw_cell_t ofw_finddevice(const char* device);
int ofw_getproplen(ofw_cell_t phandle, const char* name);
int ofw_getprop(ofw_cell_t phandle, const char* name, void* buf, int length);
int ofw_nextprop(ofw_cell_t phandle, const char* prev, void* buf);
ofw_cell_t ofw_instance_to_package(ofw_cell_t ihandle);
int ofw_package_to_path(ofw_cell_t phandle, void* buffer, int buflen);

/* memory */
void* ofw_claim(ofw_cell_t virt, ofw_cell_t size, ofw_cell_t align);
void ofw_release(ofw_cell_t virt, ofw_cell_t size);

/* control transfer */
void ofw_enter();
void ofw_exit();

#ifdef LOADER
/*
 * For the loader, calling the OFW is very straightforward, so make the
 * entry point available here - the kernel needs to jump through all
 * kinds of hoops, so let's not bother there.
 */
extern ofw_entry_t ofw_entry;
#endif

#endif /* __OPENFIRMWARE_H__ */
