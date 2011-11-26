#include <ananas/types.h>
#include <elf.h>

void
efi_relocate(addr_t dynamic, addr_t elfbase)
{
	Elf32_Rel* rel = NULL;
	int relent = 0, relsz = 0;

	for (Elf32_Dyn* dyn = (Elf32_Dyn*)dynamic; dyn->d_tag != DT_NULL; dyn++) {
		switch(dyn->d_tag) {
			case DT_RELA: /* Relocation table */
				break;
			case DT_REL:
				rel = dyn->d_un.d_ptr + elfbase;
				break;
			case DT_RELSZ:
				relsz = dyn->d_un.d_val;
				break;
			case DT_RELENT: /* Number of relocation entries */
				relent = dyn->d_un.d_val;
				break;
		}
	}

	__asm("xchgw %bx,%bx");
	__asm("mov %0,%%eax" : : "r" (rel));
	for (int n = 0; n < relent; n++) {
		switch(ELF32_R_TYPE(rel->r_info)) {
			case R_386_RELATIVE:
				*(uint32_t*)rel->r_offset += elfbase;
				break;
		}
		rel = (Elf32_Rel*)((char*)rel + relsz);
	}
}

/* vim:set ts=2 sw=2: */
