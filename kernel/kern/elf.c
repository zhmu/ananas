#include <ananas/types.h>
#include <machine/thread.h>
#include <machine/param.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/thread.h>
#include <ananas/trace.h>
#include <ananas/vfs.h>
#include <elf.h>

TRACE_SETUP;

/* XXX */
#ifdef _ARCH_PPC
#define __PowerPC__
#endif

#if defined(__i386__) || defined(__PowerPC__)
static errorcode_t
elf32_load(thread_t thread, void* priv, elf_getfunc_t obtain)
{
	errorcode_t err;
	Elf32_Ehdr ehdr;

	err = obtain(priv, &ehdr, 0, sizeof(ehdr));
	ANANAS_ERROR_RETURN(err);

	/* Perform basic ELF checks */
	if (ehdr.e_ident[EI_MAG0] != ELFMAG0 || ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr.e_ident[EI_MAG2] != ELFMAG2 || ehdr.e_ident[EI_MAG3] != ELFMAG3)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr.e_ident[EI_CLASS] != ELFCLASS32)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr.e_ident[EI_VERSION] != EV_CURRENT)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr.e_type != ET_EXEC)
		return ANANAS_ERROR(BAD_EXEC);

	/* XXX This specifically checks for i386 at the moment */
#if defined(__i386__) || defined(__amd64__)
	if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr.e_machine != EM_386)
		return ANANAS_ERROR(BAD_EXEC);
#endif
#if defined(__PowerPC__)
	if (ehdr.e_ident[EI_DATA] != ELFDATA2MSB)
		return ANANAS_ERROR(BAD_EXEC);
#endif

	/* We only support static binaries for now, so reject anything without a program table */
	if (ehdr.e_phnum == 0)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr.e_phentsize < sizeof(Elf32_Phdr))
		return ANANAS_ERROR(BAD_EXEC);

	TRACE(EXEC, INFO, "found %u program headers", ehdr.e_phnum);
	for (unsigned int i = 0; i < ehdr.e_phnum; i++) {
		Elf32_Phdr phdr;
		TRACE(EXEC, INFO, "ph %u: obtaining header from offset %u", i, ehdr.e_phoff + i * ehdr.e_phentsize);
		err = obtain(priv, &phdr, ehdr.e_phoff + i * ehdr.e_phentsize, sizeof(phdr));
		if (err != ANANAS_ERROR_NONE) {
			TRACE(EXEC, INFO, "ph %u: obtain failed: %i", i, err);
			return err;
		}
		TRACE(EXEC, INFO, "ph %u: obtained, header type=%u", i, phdr.p_type);
		if (phdr.p_type != PT_LOAD)
			continue;

		/*
		 * The program need not begin at a page-size, so we may need to adjust.
		 */
		int delta = phdr.p_vaddr % PAGE_SIZE;
		TRACE(EXEC, INFO, "ph %u: instantiating mapping for %x (%u bytes)",
		 i, (phdr.p_vaddr - delta), phdr.p_memsz + delta);
		struct THREAD_MAPPING* tm;
		/* XXX deal with mode */
		int mode = THREAD_MAP_ALLOC;
		err = thread_mapto(thread, (void*)(phdr.p_vaddr - delta), NULL, phdr.p_memsz + delta, mode, &tm);
		ANANAS_ERROR_RETURN(err);
	
		TRACE(EXEC, INFO, "ph %u: loading to 0x%x (%u bytes, file offset is %u)", i, tm->ptr + delta, phdr.p_filesz, phdr.p_offset);
		err = obtain(priv, tm->ptr + delta, phdr.p_offset, phdr.p_filesz);
		ANANAS_ERROR_RETURN(err);
	}
	TRACE(EXEC, INFO, "done, entry point is 0x%x", ehdr.e_entry);
	md_thread_set_entrypoint(thread, ehdr.e_entry);

	return ANANAS_ERROR_OK;
}
#endif /*__i386__ */

#ifdef __amd64__
static int
elf64_load(thread_t thread, void* priv, elf_getfunc_t obtain)
{
	errorcode_t err;
	Elf64_Ehdr ehdr;

	err = obtain(priv, &ehdr, 0, sizeof(ehdr));
	ANANAS_ERROR_RETURN(err);

	/* Perform basic ELF checks; must be 64 bit LSB statically-linked executable */
	if (ehdr.e_ident[EI_MAG0] != ELFMAG0 || ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr.e_ident[EI_MAG2] != ELFMAG2 || ehdr.e_ident[EI_MAG3] != ELFMAG3)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr.e_ident[EI_CLASS] != ELFCLASS64)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr.e_ident[EI_VERSION] != EV_CURRENT)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr.e_type != ET_EXEC)
		return ANANAS_ERROR(BAD_EXEC);

	/* XXX This specifically checks for amd64 at the moment */
	if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr.e_machine != EM_X86_64)
		return ANANAS_ERROR(BAD_EXEC);

	/* We only support static binaries for now, so reject anything without a program table */
	if (ehdr.e_phnum == 0)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr.e_phentsize < sizeof(Elf64_Phdr))
		return ANANAS_ERROR(BAD_EXEC);

	unsigned int i;
	for (i = 0; i < ehdr.e_phnum; i++) {
		Elf64_Phdr phdr;
		err = obtain(priv, &phdr, ehdr.e_phoff + i * ehdr.e_phentsize, sizeof(phdr));
		ANANAS_ERROR_RETURN(err);
		if (phdr.p_type != PT_LOAD)
			continue;

		/*
		 * The program need not begin at a page-size, so we may need to adjust.
		 */
		int delta = phdr.p_vaddr % PAGE_SIZE;
		struct THREAD_MAPPING* tm;
		/* XXX deal with mode */
		int mode = THREAD_MAP_ALLOC;
		err = thread_mapto(thread, (void*)(phdr.p_vaddr - delta), NULL, phdr.p_memsz + delta, mode, &tm);
		ANANAS_ERROR_RETURN(err);
		err = obtain(priv, tm->ptr + delta, phdr.p_offset, phdr.p_filesz);
		ANANAS_ERROR_RETURN(err);
	}

	md_thread_set_entrypoint(thread, ehdr.e_entry);
	return ANANAS_ERROR_OK;
}
#endif /* __amd64__ */

static errorcode_t
elf_load_filefunc(void* priv, void* buf, off_t offset, size_t len)
{
	struct VFS_FILE* f = (struct VFS_FILE*)priv;
	errorcode_t err;

	err = vfs_seek(f, offset);
	ANANAS_ERROR_RETURN(err);

	size_t amount = len;
	err = vfs_read(f, buf, &amount);
	ANANAS_ERROR_RETURN(err);

	if (amount != len)
		return ANANAS_ERROR(SHORT_READ);
	return ANANAS_ERROR_OK;
}

errorcode_t
elf_load(thread_t t, void* priv, elf_getfunc_t obtain)
{
	/*
	 * XXX this should detect the type.
	 */
	return
#if defined(__i386__ ) || defined(__PowerPC__)
		elf32_load(t, priv, obtain)
#elif defined(__amd64__)
		elf64_load(t, priv, obtain)
#else
		0
#endif
	;
}

errorcode_t
elf_load_from_file(thread_t t, struct VFS_FILE* f)
{
	return elf_load(t, f, elf_load_filefunc);
}

/* vim:set ts=2 sw=2: */
