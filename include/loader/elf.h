#ifndef __LOADER_ELF_H__
#define __LOADER_ELF_H__

enum MODULE_TYPE;
struct LOADER_MODULE;

int elf_load(enum MODULE_TYPE type, struct LOADER_MODULE* mod);

#endif /* __LOADER_ELF_H__ */
