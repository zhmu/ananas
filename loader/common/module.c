#include <ananas/types.h>
#include <ananas/bootinfo.h>
#include <loader/elf.h>
#include <loader/module.h>
#include <loader/lib.h>

struct LOADER_MODULE mod_kernel;

int
module_load_kernel()
{
	return elf_load(MOD_KERNEL, &mod_kernel);
}

int
module_load_module()
{
	struct LOADER_MODULE new_module_info;
	if (!elf_load(MOD_MODULE, &new_module_info))
		return 0;

	/* Module was successfully loaded - find a permanent place for the module info */
	struct LOADER_MODULE* mod_info = (struct LOADER_MODULE*)(addr_t)mod_kernel.mod_phys_end_addr;
	mod_kernel.mod_phys_end_addr += sizeof(struct LOADER_MODULE);
	memcpy(mod_info, &new_module_info, sizeof(new_module_info));

	/* Hook it to the chain */
	struct LOADER_MODULE* mod_chain = &mod_kernel;
	while (mod_chain->mod_next != NULL)
		mod_chain = mod_chain->mod_next;
	mod_chain->mod_next = mod_info;
	return 1;
}

int
module_load(enum MODULE_TYPE type)
{
	switch(type) {
		case MOD_KERNEL: return module_load_kernel();
		case MOD_MODULE: return module_load_module();
	}

	/* What's this? */
	return 0;
}

void
module_make_bootinfo(struct BOOTINFO** bootinfo)
{	
	/*
	 * Place the bootinfo after the kernel; this ensures it'll be mapped
	 * along with everything else and no special cases are needed.
	 */
	*bootinfo = (struct BOOTINFO*)(addr_t)mod_kernel.mod_phys_end_addr;
	mod_kernel.mod_phys_end_addr += sizeof(struct BOOTINFO);

	/* Place the kernel module information after the bootinfo */
	struct LOADER_MODULE* kernel_mod = (struct LOADER_MODULE*)(addr_t)mod_kernel.mod_phys_end_addr;
	mod_kernel.mod_phys_end_addr += sizeof(struct LOADER_MODULE);
	memcpy(kernel_mod, &mod_kernel, sizeof(struct LOADER_MODULE));

	/* Fill the bootinfo out */
	memset(*bootinfo, 0, sizeof(struct BOOTINFO));
	(*bootinfo)->bi_size = sizeof(struct BOOTINFO);
	(*bootinfo)->bi_modules = (addr_t)kernel_mod;

	printf("final address = %p\n", mod_kernel.mod_phys_end_addr);
}

/* vim:set ts=2 sw=2: */
