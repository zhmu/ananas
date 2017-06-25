#include <ananas/types.h>
#include <machine/param.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/exec.h>
#include <ananas/process.h>
#include <ananas/trace.h>
#include <ananas/mm.h>
#include <ananas/vm.h>
#include <ananas/vmspace.h>
#include <elf.h>

TRACE_SETUP;

static errorcode_t
read_data(struct DENTRY* dentry, void* buf, off_t offset, size_t len)
{
	struct VFS_FILE f;
	memset(&f, 0, sizeof(f));
	f.f_dentry = dentry;

	errorcode_t err = vfs_seek(&f, offset);
	ANANAS_ERROR_RETURN(err);

	size_t amount = len;
	err = vfs_read(&f, buf, &amount);
	ANANAS_ERROR_RETURN(err);

	if (amount != len)
		return ANANAS_ERROR(SHORT_READ);
	return ananas_success();
}

#ifdef __amd64__
static errorcode_t
elf64_load(vmspace_t* vs, struct DENTRY* dentry, addr_t* exec_addr)
{
	errorcode_t err;
	Elf64_Ehdr ehdr;

	err = read_data(dentry, &ehdr, 0, sizeof(ehdr));
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
#ifdef LITTLE_ENDIAN
	if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB)
		return ANANAS_ERROR(BAD_EXEC);
#endif
#ifdef BIG_ENDIAN
	if (ehdr.e_ident[EI_DATA] != ELFDATA2MSB)
		return ANANAS_ERROR(BAD_EXEC);
#endif
#ifdef __amd64__
	if (ehdr.e_machine != EM_X86_64)
		return ANANAS_ERROR(BAD_EXEC);
#endif

	/* We only support static binaries for now, so reject anything without a program table */
	if (ehdr.e_phnum == 0)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr.e_phentsize < sizeof(Elf64_Phdr))
		return ANANAS_ERROR(BAD_EXEC);

	/* Map all program headers one by one */
	for (unsigned int i = 0; i < ehdr.e_phnum; i++) {
		Elf64_Phdr phdr;
		err = read_data(dentry, &phdr, ehdr.e_phoff + i * ehdr.e_phentsize, sizeof(phdr));
		ANANAS_ERROR_RETURN(err);
		if (phdr.p_type != PT_LOAD)
			continue;

		/* Construct the flags for the actual mapping */
		unsigned int flags = VM_FLAG_FAULT | VM_FLAG_USER;
		if (phdr.p_flags & PF_R)
			flags |= VM_FLAG_READ;
		if (phdr.p_flags & PF_W)
			flags |= VM_FLAG_WRITE;
		if (phdr.p_flags & PF_X)
			flags |= VM_FLAG_EXECUTE;

		// If this mapping is writable, do not share it
		if (flags & VM_FLAG_WRITE)
			flags |= VM_FLAG_PRIVATE;

		/*
		 * The program need not begin at a page-size, so we may need to adjust.
		 */
		vmarea_t* va;
		addr_t virt_begin = ROUND_DOWN(phdr.p_vaddr, PAGE_SIZE);
		addr_t virt_end   = ROUND_UP((phdr.p_vaddr + phdr.p_memsz), PAGE_SIZE);
		off_t vskip = phdr.p_vaddr - virt_begin;
		TRACE(EXEC, INFO, "elf map: vbegin=%p vend=%p vskip=%d offset=%d filesz=%d\n", virt_begin, virt_end, vskip, phdr.p_offset, phdr.p_filesz);
		err = vmspace_mapto_dentry(vs, virt_begin, vskip, virt_end - virt_begin, dentry, phdr.p_offset, phdr.p_filesz, flags, &va);
		ANANAS_ERROR_RETURN(err);
	}

	*exec_addr = ehdr.e_entry;
	return ananas_success();
}

EXECUTABLE_FORMAT("elf64", elf64_load);
#endif /* __amd64__ */

/* vim:set ts=2 sw=2: */
