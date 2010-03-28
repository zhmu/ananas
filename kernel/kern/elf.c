#include <sys/types.h>
#include <machine/thread.h>
#include <machine/param.h>
#include <sys/lib.h>
#include <sys/thread.h>
#include <sys/vfs.h>
#include <elf.h>

#ifdef __i386__
static int
elf32_load(thread_t thread, void* priv, elf_getfunc_t obtain)
{
	Elf32_Ehdr ehdr;
	if (!obtain(priv, &ehdr, 0, sizeof(ehdr)))
		return 0;

	/* Perform basic ELF checks */
	if (ehdr.e_ident[EI_MAG0] != ELFMAG0 || ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr.e_ident[EI_MAG2] != ELFMAG2 || ehdr.e_ident[EI_MAG3] != ELFMAG3)
		return 0;
	if (ehdr.e_ident[EI_CLASS] != ELFCLASS32)
		return 0;
	if (ehdr.e_ident[EI_VERSION] != EV_CURRENT)
		return 0;
	if (ehdr.e_type != ET_EXEC)
		return 0;

	/* XXX This specifically checks for i386 at the moment */
	if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB)
		return 0;
	if (ehdr.e_machine != EM_386)
		return 0;

	/* We only support static binaries for now, so reject anything without a program table */
	if (ehdr.e_phnum == 0)
		return 0;
	if (ehdr.e_phentsize < sizeof(Elf32_Phdr))
		return 0;

	for (unsigned int i = 0; i < ehdr.e_phnum; i++) {
		Elf32_Phdr phdr;
		if (!obtain(priv, &phdr, ehdr.e_phoff + i * ehdr.e_phentsize, sizeof(phdr)))
			return 0;
		if (phdr.p_type != PT_LOAD)
			continue;

		/*
		 * The program need not begin at a page-size, so we may need to adjust.
		 */
		int delta = phdr.p_vaddr % PAGE_SIZE;
		struct THREAD_MAPPING* tm = thread_mapto(thread, (void*)(phdr.p_vaddr - delta), NULL, phdr.p_memsz + delta, THREAD_MAP_ALLOC);
		if (!obtain(priv, tm->ptr, phdr.p_offset + delta, phdr.p_filesz))
			return 0;
	}
	md_thread_set_entrypoint(thread, ehdr.e_entry);

	return 1;
}
#endif /*__i386__ */

#ifdef __amd64__
static int
elf64_load(thread_t thread, void* priv, elf_getfunc_t obtain)
{
	Elf64_Ehdr ehdr;
	if (!obtain(priv, &ehdr, 0, sizeof(ehdr)))
		return 0;

	/* Perform basic ELF checks; must be 64 bit LSB statically-linked executable */
	if (ehdr.e_ident[EI_MAG0] != ELFMAG0 || ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr.e_ident[EI_MAG2] != ELFMAG2 || ehdr.e_ident[EI_MAG3] != ELFMAG3)
		return 0;
	if (ehdr.e_ident[EI_CLASS] != ELFCLASS64)
		return 0;
	if (ehdr.e_ident[EI_VERSION] != EV_CURRENT)
		return 0;
	if (ehdr.e_type != ET_EXEC)
		return 0;

	/* XXX This specifically checks for amd64 at the moment */
	if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB)
		return 0;
	if (ehdr.e_machine != EM_X86_64)
		return 0;

	/* We only support static binaries for now, so reject anything without a program table */
	if (ehdr.e_phnum == 0)
		return 0;
	if (ehdr.e_phentsize < sizeof(Elf64_Phdr))
		return 0;

	unsigned int i;
	for (i = 0; i < ehdr.e_phnum; i++) {
		Elf64_Phdr phdr;
		if (!obtain(priv, &phdr, ehdr.e_phoff + i * ehdr.e_phentsize, sizeof(phdr)))
			return 0;
		if (phdr.p_type != PT_LOAD)
			continue;

		/*
		 * The program need not begin at a page-size, so we may need to adjust.
		 */
		int delta = phdr.p_vaddr % PAGE_SIZE;
		struct THREAD_MAPPING* tm = thread_mapto(thread, (void*)phdr.p_vaddr - delta, NULL, phdr.p_memsz + delta, THREAD_MAP_ALLOC);
		if (!obtain(priv, tm->ptr, phdr.p_offset + delta, phdr.p_filesz))
			return 0;
	}

	md_thread_set_entrypoint(thread, ehdr.e_entry);
	return 1;
}
#endif /* __amd64__ */

static int
elf_load_filefunc(void* priv, void* buf, off_t offset, size_t len)
{
	struct VFS_FILE* f = (struct VFS_FILE*)priv;
	if (!vfs_seek(f, offset))
		return 0;
	return vfs_read(f, buf, len);
}

int
elf_load(thread_t t, void* priv, elf_getfunc_t obtain)
{
	/*
	 * XXX this should detect the type.
	 */
	return
#ifdef __i386__
		elf32_load(t, priv, obtain)
#elif defined(__amd64__)
		elf64_load(t, priv, obtain)
#else
		0
#endif
	;
}

int
elf_load_from_file(thread_t t, struct VFS_FILE* f)
{
	return elf_load(t, f, elf_load_filefunc);
}

/* vim:set ts=2 sw=2: */
