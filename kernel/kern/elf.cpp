#include <ananas/types.h>
#include <machine/param.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/exec.h>
#include <ananas/process.h>
#include <ananas/trace.h>
#include <ananas/kmem.h>
#include <ananas/mm.h>
#include <ananas/vm.h>
#include <ananas/vmspace.h>
#include <ananas/elfinfo.h>
#include <elf.h>

TRACE_SETUP;

// XXX The next constants are a kludge - we need to have a nicer mechanism to allocate
//     virtual address space (vmspace should be extended for dynamic mappings)
#define INTERPRETER_BASE 0xf000000
#define PHDR_BASE 0xd0000000
#define ELFINFO_BASE 0xe000000

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
elf64_load_ph(vmspace_t* vs, struct DENTRY* dentry, const Elf64_Phdr& phdr, addr_t rbase)
{
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
	 * The program need not begin at a page-size, so we may need to adjust. XXX we need more checks here,
	 * for example to see if we can even map this and if we aren't going out of bounds somewhere.
	 */
	vmarea_t* va;
	addr_t virt_begin = ROUND_DOWN(phdr.p_vaddr, PAGE_SIZE);
	addr_t virt_end   = ROUND_UP((phdr.p_vaddr + phdr.p_memsz), PAGE_SIZE);
	off_t vskip = phdr.p_vaddr - virt_begin;
	TRACE(EXEC, INFO, "elf map: vbegin=%p vend=%p vskip=%d offset=%d filesz=%d\n", virt_begin, virt_end, vskip, phdr.p_offset, phdr.p_filesz);
	return vmspace_mapto_dentry(vs, rbase + virt_begin, vskip, virt_end - virt_begin, dentry, phdr.p_offset - vskip, phdr.p_filesz, flags, &va);
}

static errorcode_t
elf64_check_header(const Elf64_Ehdr& ehdr)
{
	/* Perform basic ELF checks; must be statically-linked executable */
	if (ehdr.e_ident[EI_MAG0] != ELFMAG0 || ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr.e_ident[EI_MAG2] != ELFMAG2 || ehdr.e_ident[EI_MAG3] != ELFMAG3)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr.e_ident[EI_VERSION] != EV_CURRENT)
		return ANANAS_ERROR(BAD_EXEC);

	/* XXX This specifically checks for amd64 at the moment */
	if (ehdr.e_ident[EI_CLASS] != ELFCLASS64)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr.e_machine != EM_X86_64)
		return ANANAS_ERROR(BAD_EXEC);

	/* We only support static binaries here, so reject anything without a program table */
	if (ehdr.e_phnum == 0)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr.e_phentsize < sizeof(Elf64_Phdr))
		return ANANAS_ERROR(BAD_EXEC);

	return ananas_success();
}

static errorcode_t
elf64_load_file(vmspace_t* vs, struct DENTRY* dentry, addr_t rbase, addr_t* exec_addr)
{
	errorcode_t err;
	Elf64_Ehdr ehdr;

	err = read_data(dentry, &ehdr, 0, sizeof(ehdr));
	ANANAS_ERROR_RETURN(err);

	err = elf64_check_header(ehdr);
	ANANAS_ERROR_RETURN(err);

	/* Process all program headers one by one */
	for (unsigned int i = 0; i < ehdr.e_phnum; i++) {
		Elf64_Phdr phdr;
		err = read_data(dentry, &phdr, ehdr.e_phoff + i * ehdr.e_phentsize, sizeof(phdr));
		ANANAS_ERROR_RETURN(err);
		if(phdr.p_type != PT_LOAD)
			continue;

		err = elf64_load_ph(vs, dentry, phdr, rbase);
		ANANAS_ERROR_RETURN(err);
	}

	*exec_addr = ehdr.e_entry;
	return ananas_success();
}

static errorcode_t
elf64_load(vmspace_t* vs, struct DENTRY* dentry, addr_t* exec_addr, register_t* exec_arg)
{
	*exec_arg = 0;

	// Load the file - this checks the ELF header as well
	errorcode_t err = elf64_load_file(vs, dentry, 0, exec_addr);
	ANANAS_ERROR_RETURN(err);

	// Fetch the ELF header (again - this could be nicer...)
	Elf64_Ehdr ehdr;
	err = read_data(dentry, &ehdr, 0, sizeof(ehdr));
	ANANAS_ERROR_RETURN(err);

	// Only accept executables here
	if (ehdr.e_type != ET_EXEC)
		return ANANAS_ERROR(BAD_EXEC);

	// This went okay - see if there any other interesting headers here
	char* interp = nullptr;
	for (unsigned int i = 0; i < ehdr.e_phnum; i++) {
		Elf64_Phdr phdr;
		err = read_data(dentry, &phdr, ehdr.e_phoff + i * ehdr.e_phentsize, sizeof(phdr));
		ANANAS_ERROR_RETURN(err);
		switch(phdr.p_type) {
			case PT_INTERP: {
				if (interp != nullptr) {
					kfree(interp);
					kprintf("elf64_load: multiple interp headers\n");
					return ANANAS_ERROR(BAD_EXEC);
				}

				if (phdr.p_filesz >= PAGE_SIZE) {
					kprintf("elf64_load: interp too large\n");
					return ANANAS_ERROR(BAD_EXEC);
				}

				// Read the interpreter from disk
				interp = static_cast<char*>(kmalloc(phdr.p_filesz + 1));
				interp[phdr.p_filesz] = '\0';

				err = read_data(dentry, interp, phdr.p_offset, phdr.p_filesz);
				ANANAS_ERROR_RETURN(err); // XXX leaks interp
				break;
			}
		}
	 }

	if (interp != nullptr) {
		// We need to use an interpreter to load this
		struct VFS_FILE interp_file;
		// Note that the nullptr requires interp to be absolute
		err = vfs_open(interp, nullptr, &interp_file);
		if (ananas_is_failure(err)) {
			kprintf("unable to load ELF interpreter '%s': %d\n", interp, err);
			kfree(interp);
			ANANAS_ERROR_RETURN(err);
		}
		kfree(interp);

		// Load the interpreter ELF file
		addr_t interp_rbase = INTERPRETER_BASE;
		addr_t interp_entry;
		err = elf64_load_file(vs, interp_file.f_dentry, interp_rbase, &interp_entry);
		vfs_close(&interp_file); // we don't need it anymore
		ANANAS_ERROR_RETURN(err);

		/*
		* Map the program headers in memory - we need to process them anyway, and we
		* want to hand them over the the executable itself, as the dynamic loader
		* will likely need them.
		*
		* XXX Note that _we_ can't use the mappings here because vs is likely not
		*     our current vmspace...
		*/
		vmarea_t* va_phdr;
		{
			size_t phdr_len = ehdr.e_phnum * ehdr.e_phentsize;
			err = vmspace_mapto_dentry(vs, PHDR_BASE, ehdr.e_phoff & (PAGE_SIZE - 1), phdr_len, dentry, ehdr.e_phoff & ~(PAGE_SIZE - 1), phdr_len, VM_FLAG_READ | VM_FLAG_USER, &va_phdr);
			ANANAS_ERROR_RETURN(err);
		}

		/*
		 * As we have a binary with an interpreter, prepare the information
		 * structure. Ideally, we should place this on the stack somewhere
		 * as it's no longer relevant once the loader finished, yet...
		 */

		// Create a mapping for the ELF information
		vmarea_t* va;
		err = vmspace_mapto(vs, ELFINFO_BASE, sizeof(struct ANANAS_ELF_INFO), VM_FLAG_READ | VM_FLAG_USER, &va);
		ANANAS_ERROR_RETURN(err);
		*exec_arg = va->va_virt;

		// Now assign a page to there
		struct VM_PAGE* vp = vmpage_create_private(va, VM_PAGE_FLAG_PRIVATE | VM_PAGE_FLAG_READONLY);
		vp->vp_vaddr = ELFINFO_BASE;
		auto elf_info = static_cast<struct ANANAS_ELF_INFO*>(kmem_map(page_get_paddr(vmpage_get_page(vp)), sizeof(struct ANANAS_ELF_INFO), VM_FLAG_READ | VM_FLAG_WRITE));
		vmpage_unlock(vp);

		// And fil it out
		memset(elf_info, 0, PAGE_SIZE);
		elf_info->ei_size = sizeof(*elf_info);
		elf_info->ei_interpreter_base = interp_rbase;
		elf_info->ei_entry = *exec_addr;
		elf_info->ei_phdr = va_phdr->va_virt;
		elf_info->ei_phdr_entries = ehdr.e_phnum;
		kmem_unmap(elf_info, sizeof(struct ANANAS_ELF_INFO));

		// Override the load address
		*exec_addr = interp_entry + interp_rbase;
		kprintf(">> exec addr %p virt %p\n", *exec_addr, *exec_arg);
	}

	return ananas_success();
}

EXECUTABLE_FORMAT("elf64", elf64_load);
#endif /* __amd64__ */

/* vim:set ts=2 sw=2: */
