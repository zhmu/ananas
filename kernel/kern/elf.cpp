#include <machine/param.h>
#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/elf.h>
#include <ananas/elfinfo.h>
#include "kernel/exec.h"
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/vmspace.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/types.h"
#include "kernel/vm.h"

TRACE_SETUP;

// XXX The next constants are a kludge - we need to have a nicer mechanism to allocate
//     virtual address space (vmspace should be extended for dynamic mappings)
#define INTERPRETER_BASE 0xf000000
#define PHDR_BASE 0xd0000000
#define ELFINFO_BASE 0xe000000

addr_t last_exec_addr;

static Result
read_data(DEntry& dentry, void* buf, off_t offset, size_t len)
{
	struct VFS_FILE f;
	memset(&f, 0, sizeof(f));
	f.f_dentry = &dentry;

	RESULT_PROPAGATE_FAILURE(
		vfs_seek(&f, offset)
	);

	size_t amount = len;
	RESULT_PROPAGATE_FAILURE(
		vfs_read(&f, buf, &amount)
	);

	if (amount != len)
		return RESULT_MAKE_FAILURE(EOVERFLOW); // XXX is this sane?
	return Result::Success();
}

#ifdef __amd64__
static Result
elf64_load_ph(VMSpace& vs, DEntry& dentry, const Elf64_Phdr& phdr, addr_t rbase)
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
	 * We can only map things at PAGE_SIZE offsets and these must start at some
	 * PAGE_SIZE-aligned offset.
	 *
	 * Note that we may map extra bytes here, this shouldn't really matter since
	 * everything we map here won't make it back to disk, and it is likely
	 * available anyway (in theory, the file could be execute-only, but you can
	 * still read the extra pieces in memory...)
	 *
	 * Finally, we have a separate mapping for things outside of the file as
	 * that makes stuff easier to manage (plus, mapping extending past the file
	 * length may be silently truncated)
	 *
	 *  virt_extra                      virt_end
	 * <---------->                        /
	 * +----------+------------+----------+
	 * |          |XXXXXXXXXXXX|          |
	 * +----------+------------+----------+
	 *           /             |
	 *       virt_begin    ROUND_UP(v_addr + filesz)
	 */
	VMArea* va;
	addr_t virt_begin = ROUND_DOWN(phdr.p_vaddr, PAGE_SIZE);
	addr_t virt_end   = ROUND_UP((phdr.p_vaddr + phdr.p_filesz), PAGE_SIZE);
	addr_t virt_extra = phdr.p_vaddr - virt_begin;
	addr_t doffset = phdr.p_offset - virt_extra;
	size_t filesz = phdr.p_filesz + virt_extra;
	if (doffset & (PAGE_SIZE - 1)) {
		kprintf("elf64_load_ph: refusing to map offset %d not a page-multiple\n", doffset);
		return RESULT_MAKE_FAILURE(ENOEXEC);
	}

	// First step is to map the dentry-backed data
	RESULT_PROPAGATE_FAILURE(
		vmspace_mapto_dentry(vs, rbase + virt_begin, virt_end - virt_begin, dentry, doffset, filesz, flags, va)
	);
	if (phdr.p_filesz == phdr.p_memsz)
		return Result::Success();

	// Now map the part that isn't dentry-backed, if we have enough
	addr_t v_extra = virt_end;
	addr_t v_extra_len = ROUND_UP(phdr.p_vaddr + phdr.p_memsz - v_extra, PAGE_SIZE);
	if (v_extra_len == 0)
		return Result::Success();
	return vmspace_mapto(vs, rbase + v_extra, v_extra_len, flags, va);
}

static Result
elf64_check_header(const Elf64_Ehdr& ehdr)
{
	/* Perform basic ELF checks; must be statically-linked executable */
	if (ehdr.e_ident[EI_MAG0] != ELFMAG0 || ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr.e_ident[EI_MAG2] != ELFMAG2 || ehdr.e_ident[EI_MAG3] != ELFMAG3)
		return RESULT_MAKE_FAILURE(ENOEXEC);
	if (ehdr.e_ident[EI_VERSION] != EV_CURRENT)
		return RESULT_MAKE_FAILURE(ENOEXEC);

	/* XXX This specifically checks for amd64 at the moment */
	if (ehdr.e_ident[EI_CLASS] != ELFCLASS64)
		return RESULT_MAKE_FAILURE(ENOEXEC);
	if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB)
		return RESULT_MAKE_FAILURE(ENOEXEC);
	if (ehdr.e_machine != EM_X86_64)
		return RESULT_MAKE_FAILURE(ENOEXEC);

	/* We only support static binaries here, so reject anything without a program table */
	if (ehdr.e_phnum == 0)
		return RESULT_MAKE_FAILURE(ENOEXEC);
	if (ehdr.e_phentsize < sizeof(Elf64_Phdr))
		return RESULT_MAKE_FAILURE(ENOEXEC);

	return Result::Success();
}

static Result
elf64_load_file(VMSpace& vs, DEntry& dentry, addr_t rbase, addr_t* exec_addr)
{
	Elf64_Ehdr ehdr;

	RESULT_PROPAGATE_FAILURE(
		read_data(dentry, &ehdr, 0, sizeof(ehdr))
	);

	RESULT_PROPAGATE_FAILURE(
		elf64_check_header(ehdr)
	);

	/* Process all program headers one by one */
	for (unsigned int i = 0; i < ehdr.e_phnum; i++) {
		Elf64_Phdr phdr;
		RESULT_PROPAGATE_FAILURE(
			read_data(dentry, &phdr, ehdr.e_phoff + i * ehdr.e_phentsize, sizeof(phdr))
		);
		if(phdr.p_type != PT_LOAD)
			continue;

		RESULT_PROPAGATE_FAILURE(
			elf64_load_ph(vs, dentry, phdr, rbase)
		);
	}

	*exec_addr = ehdr.e_entry;
	last_exec_addr = *exec_addr;
	return Result::Success();
}

static Result
elf64_load(VMSpace& vs, DEntry& dentry, addr_t* exec_addr, register_t* exec_arg)
{
	*exec_arg = 0;

	// Load the file - this checks the ELF header as well
	RESULT_PROPAGATE_FAILURE(
		elf64_load_file(vs, dentry, 0, exec_addr)
	);

	// Fetch the ELF header (again - this could be nicer...)
	Elf64_Ehdr ehdr;
	RESULT_PROPAGATE_FAILURE(
		read_data(dentry, &ehdr, 0, sizeof(ehdr))
	);

	// Only accept executables here
	if (ehdr.e_type != ET_EXEC)
		return RESULT_MAKE_FAILURE(ENOEXEC);

	// This went okay - see if there any other interesting headers here
	char* interp = nullptr;
	for (unsigned int i = 0; i < ehdr.e_phnum; i++) {
		Elf64_Phdr phdr;
		RESULT_PROPAGATE_FAILURE(
			read_data(dentry, &phdr, ehdr.e_phoff + i * ehdr.e_phentsize, sizeof(phdr))
		);
		switch(phdr.p_type) {
			case PT_INTERP: {
				if (interp != nullptr) {
					kfree(interp);
					kprintf("elf64_load: multiple interp headers\n");
					return RESULT_MAKE_FAILURE(ENOEXEC);
				}

				if (phdr.p_filesz >= PAGE_SIZE) {
					kprintf("elf64_load: interp too large\n");
					return RESULT_MAKE_FAILURE(ENOEXEC);
				}

				// Read the interpreter from disk
				interp = static_cast<char*>(kmalloc(phdr.p_filesz + 1));
				interp[phdr.p_filesz] = '\0';

				RESULT_PROPAGATE_FAILURE(
					read_data(dentry, interp, phdr.p_offset, phdr.p_filesz)
				); // XXX leaks interp
				break;
			}
		}
	 }

	if (interp != nullptr) {
		// We need to use an interpreter to load this
		struct VFS_FILE interp_file;
		// Note that the nullptr requires interp to be absolute
		if (auto result = vfs_open(interp, nullptr, &interp_file); result.IsFailure()) {
			kprintf("unable to load ELF interpreter '%s': %d\n", interp, result.AsStatusCode());
			kfree(interp);
			return result;
		}
		kfree(interp);

		// Load the interpreter ELF file
		addr_t interp_rbase = INTERPRETER_BASE;
		addr_t interp_entry;
		{
			auto result = elf64_load_file(vs, *interp_file.f_dentry, interp_rbase, &interp_entry);
			vfs_close(&interp_file); // we don't need it anymore
			if (result.IsFailure())
				return result;
		}

		/*
		* Map the program headers in memory - we need to process them anyway, and we
		* want to hand them over the the executable itself, as the dynamic loader
		* will likely need them.
		*
		* XXX Note that _we_ can't use the mappings here because vs is likely not
		*     our current vmspace...
		*/
		VMArea* va_phdr;
		{
			size_t phdr_len = ehdr.e_phnum * ehdr.e_phentsize;
			RESULT_PROPAGATE_FAILURE(
				vmspace_mapto_dentry(vs, PHDR_BASE, phdr_len, dentry, ehdr.e_phoff & ~(PAGE_SIZE - 1), phdr_len, VM_FLAG_READ | VM_FLAG_USER, va_phdr)
			);
		}

		/*
		 * As we have a binary with an interpreter, prepare the information
		 * structure. Ideally, we should place this on the stack somewhere
		 * as it's no longer relevant once the loader finished, yet...
		 */

		// Create a mapping for the ELF information
		VMArea* va;
		RESULT_PROPAGATE_FAILURE(
			vmspace_mapto(vs, ELFINFO_BASE, sizeof(struct ANANAS_ELF_INFO), VM_FLAG_READ | VM_FLAG_USER, va)
		);
		*exec_arg = va->va_virt;

		// Now assign a page to there and map it into the vmspae
		VMPage& vp = vmpage_create_private(va, VM_PAGE_FLAG_PRIVATE | VM_PAGE_FLAG_READONLY);
		vp.vp_vaddr = ELFINFO_BASE;
		auto elf_info = static_cast<struct ANANAS_ELF_INFO*>(kmem_map(vp.GetPage()->GetPhysicalAddress(), sizeof(struct ANANAS_ELF_INFO), VM_FLAG_READ | VM_FLAG_WRITE));
		vmpage_map(vs, *va, vp);
		vp.Unlock();

		// And fill it out
		memset(elf_info, 0, PAGE_SIZE);
		elf_info->ei_size = sizeof(*elf_info);
		elf_info->ei_interpreter_base = interp_rbase;
		elf_info->ei_entry = *exec_addr;
		elf_info->ei_phdr = va_phdr->va_virt + (ehdr.e_phoff & (PAGE_SIZE - 1));
		elf_info->ei_phdr_entries = ehdr.e_phnum;
		kmem_unmap(elf_info, sizeof(struct ANANAS_ELF_INFO));

		// Override the load address
		*exec_addr = interp_entry + interp_rbase;
	}

	return Result::Success();
}

EXECUTABLE_FORMAT("elf64", elf64_load);
#endif /* __amd64__ */

/* vim:set ts=2 sw=2: */
