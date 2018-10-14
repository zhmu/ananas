#include <machine/param.h>
#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/elf.h>
#include "kernel/exec.h"
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/process.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vmarea.h"
#include "kernel/vmspace.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/types.h"
#include "kernel/vm.h"
#include "kernel-md/md.h"
#include "kernel-md/param.h"

TRACE_SETUP;

// XXX The next constants are a kludge - we need to have a nicer mechanism to allocate
//     virtual address space (vmspace should be extended for dynamic mappings)
#define INTERPRETER_BASE 0xf000000
#define PHDR_BASE 0xd0000000

namespace
{

template<typename T>
void place(addr_t& ptr, const T& data, size_t n)
{
		memcpy(reinterpret_cast<void*>(ptr), &data, n);
		ptr += n;
}

template<typename T>
void place(addr_t& ptr, const T& data)
{
	place(ptr, data, sizeof(data));
}

size_t CalculateVectorStorageInBytes(const char* p[], size_t& num_entries)
{
	size_t n = 0;
	num_entries = 0;
	for (/* nothing */; *p != nullptr; p++) {
		n += strlen(*p) + 1 /* terminating \0 */;
		num_entries++;
	}
	return n;
}

static Result
read_data(DEntry& dentry, void* buf, off_t offset, size_t len)
{
	struct VFS_FILE f;
	memset(&f, 0, sizeof(f));
	f.f_dentry = &dentry;

	RESULT_PROPAGATE_FAILURE(
		vfs_seek(&f, offset)
	);

	Result result = vfs_read(&f, buf, len);
	if (result.IsFailure())
		return result;

	size_t numread = result.AsValue();
	if (numread != len)
		return RESULT_MAKE_FAILURE(EIO);
	return Result::Success();
}

static Result
LoadEhdr(DEntry& dentry, Elf64_Ehdr& ehdr)
{
	RESULT_PROPAGATE_FAILURE(
		read_data(dentry, &ehdr, 0, sizeof(ehdr))
	);

	// Perform basic ELF checks: check magic/version
	if (ehdr.e_ident[EI_MAG0] != ELFMAG0 || ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr.e_ident[EI_MAG2] != ELFMAG2 || ehdr.e_ident[EI_MAG3] != ELFMAG3)
		return RESULT_MAKE_FAILURE(ENOEXEC);
	if (ehdr.e_ident[EI_VERSION] != EV_CURRENT)
		return RESULT_MAKE_FAILURE(ENOEXEC);

	// XXX This specifically checks for amd64 at the moment
	if (ehdr.e_ident[EI_CLASS] != ELFCLASS64)
		return RESULT_MAKE_FAILURE(ENOEXEC);
	if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB)
		return RESULT_MAKE_FAILURE(ENOEXEC);
	if (ehdr.e_machine != EM_X86_64)
		return RESULT_MAKE_FAILURE(ENOEXEC);

	// There must be something to load
	if (ehdr.e_phnum == 0)
		return RESULT_MAKE_FAILURE(ENOEXEC);
	if (ehdr.e_phentsize < sizeof(Elf64_Phdr))
		return RESULT_MAKE_FAILURE(ENOEXEC);

	return Result::Success();
}

} // unnamed namespace

struct ELF64Loader final : IExecutor
{
	struct AuxArgs
	{
		addr_t	aa_interpreter_base = 0;
		addr_t	aa_phdr = 0;
		size_t	aa_phdr_entries = 0;
		addr_t	aa_entry = 0;
		addr_t	aa_rip = 0;
	};

	Result Verify(DEntry& dentry) override;
	Result Load(VMSpace& vs, DEntry& dentry, void*& auxargs) override;
	Result PrepareForExecute(VMSpace& vs, Thread& t, void* auxargs, const char* argv[], const char* envp[]) override;

private:
	Result LoadPH(VMSpace& vs, DEntry& dentry, const Elf64_Phdr& phdr, addr_t rbase) const;
	Result LoadFile(VMSpace& vs, DEntry& dentry, const Elf64_Ehdr& ehdr, addr_t rbase, addr_t& exec_addr) const;
} elf64Loader;

Result
ELF64Loader::LoadPH(VMSpace& vs, DEntry& dentry, const Elf64_Phdr& phdr, addr_t rbase) const
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
	VMArea* va;
	RESULT_PROPAGATE_FAILURE(
		vs.MapToDentry(rbase + virt_begin, virt_end - virt_begin, dentry, doffset, filesz, flags, va)
	);
	if (phdr.p_filesz == phdr.p_memsz)
		return Result::Success();

	// Now map the part that isn't dentry-backed, if we have enough
	addr_t v_extra = virt_end;
	addr_t v_extra_len = ROUND_UP(phdr.p_vaddr + phdr.p_memsz - v_extra, PAGE_SIZE);
	if (v_extra_len == 0)
		return Result::Success();
	return vs.MapTo(rbase + v_extra, v_extra_len, flags, va);
}

Result
ELF64Loader::LoadFile(VMSpace& vs, DEntry& dentry, const Elf64_Ehdr& ehdr, addr_t rbase, addr_t& exec_addr) const
{
	/* Process all program headers one by one */
	for (unsigned int i = 0; i < ehdr.e_phnum; i++) {
		Elf64_Phdr phdr;
		RESULT_PROPAGATE_FAILURE(
			read_data(dentry, &phdr, ehdr.e_phoff + i * ehdr.e_phentsize, sizeof(phdr))
		);
		if(phdr.p_type != PT_LOAD)
			continue;

		RESULT_PROPAGATE_FAILURE(
			LoadPH(vs, dentry, phdr, rbase)
		);
	}

	exec_addr = ehdr.e_entry;
	return Result::Success();
}

Result
ELF64Loader::Verify(DEntry& dentry)
{
	// Fetch the ELF header (again - this could be nicer...)
	Elf64_Ehdr ehdr;
	if (auto result = LoadEhdr(dentry, ehdr); result.IsFailure())
		return result;

	// Only accept executables here
	if (ehdr.e_type != ET_EXEC)
		return RESULT_MAKE_FAILURE(ENOEXEC);

	return Result::Success();
}

Result
ELF64Loader::Load(VMSpace& vs, DEntry& dentry, void*& aa)
{
	// Fetch the ELF header (again - this could be nicer...)
	Elf64_Ehdr ehdr;
	if (auto result = LoadEhdr(dentry, ehdr); result.IsFailure())
		return result;

	// Load the file
	addr_t exec_addr;
	RESULT_PROPAGATE_FAILURE(
		LoadFile(vs, dentry, ehdr, 0, exec_addr)
	);

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
				if (auto result = read_data(dentry, interp, phdr.p_offset, phdr.p_filesz); result.IsFailure()) {
					kfree(interp);
					return result;
				}
			}
		}
	}

	// Initialize auxiliary arguments; these will be passed using the auxv mechanism
	auto auxargs = new AuxArgs;
	auxargs->aa_entry = exec_addr;

	if (interp != nullptr) {
		// We need to use an interpreter to load this
		struct VFS_FILE interp_file;
		// Note that the nullptr requires interp to be absolute
		if (auto result = vfs_open(nullptr, interp, nullptr, &interp_file); result.IsFailure()) {
			kprintf("unable to load ELF interpreter '%s': %d\n", interp, result.AsStatusCode());
			kfree(interp);
			return result;
		}
		kfree(interp);

		// Load/verify the header
		Elf64_Ehdr interp_ehdr;
		if (auto result = LoadEhdr(*interp_file.f_dentry, interp_ehdr); result.IsFailure()) {
			vfs_close(nullptr, &interp_file); // we don't need it anymore
			return result;
		}

		// Load the interpreter ELF file
		addr_t interp_rbase = INTERPRETER_BASE;
		addr_t interp_entry;
		{
			auto result = LoadFile(vs, *interp_file.f_dentry, interp_ehdr, interp_rbase, interp_entry);
			vfs_close(nullptr, &interp_file); // we don't need it anymore
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
				vs.MapToDentry(PHDR_BASE, phdr_len, dentry, ehdr.e_phoff & ~(PAGE_SIZE - 1), phdr_len, VM_FLAG_READ | VM_FLAG_USER, va_phdr)
			);
		}

		// Add auxiliary arguments for dynamic executables
		auxargs->aa_interpreter_base = interp_rbase;
		auxargs->aa_phdr = va_phdr->va_virt + (ehdr.e_phoff & (PAGE_SIZE - 1));
		auxargs->aa_phdr_entries = ehdr.e_phnum;

		// Invoke the interpreter instead of the executable; it can use AT_ENTRY to
		// determine the executable entry point
		auxargs->aa_rip = interp_entry + interp_rbase;
	} else {
		// Directly invoke the static executable; no interpreter present
		auxargs->aa_rip = exec_addr;
	}

	aa = static_cast<void*>(auxargs);
	return Result::Success();
}

Result
ELF64Loader::PrepareForExecute(VMSpace& vs, Thread& t, void* aa, const char* argv[], const char* envp[])
{
	auto auxargs = static_cast<AuxArgs*>(aa);

	/*
	 * Allocate userland stack here - there are a few reasons to do that here:
	 *
	 * (1) We can look at the ELF file to check what kind of stack we need to
	 *     allocate (execute-allowed or not, etc)
	 * (2) All mappings have been made, so we can pick a nice spot.
	 * (3) This will only be called for userland programs, so we can avoid the
	 *     hairy details of stacks in the common Thread code.
	 * (4) We'll be needing write access to the stack anyway to fill it with
	 *     auxv.
	 * (5) We do not have to special-case the userland stack for cloning/exec.
	 */
	auto proc = t.t_process;

	VMArea* va;
	vs.MapTo(USERLAND_STACK_ADDR, THREAD_STACK_SIZE, VM_FLAG_USER | VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_FAULT | VM_FLAG_MD, va);

	// Pre-fault the first page so that we can put stuff in it
	constexpr auto stack_end = USERLAND_STACK_ADDR + THREAD_STACK_SIZE;
	VMPage& stack_vp = va->AllocatePrivatePage(stack_end - PAGE_SIZE, VM_PAGE_FLAG_PRIVATE);
	stack_vp.Map(vs, *va);

	/*
	 * Calculate the amount of space we need; the ELF ABI specifies the stack to
	 * be laid out as follows:
	 *
   * padding_bytes_needed + 0              ends at stack_end
   * data_bytes_needed    + <data>         contains strings for argv/envp
   *                      + AT_NULL
   *                      | auxv[...
	 * record_bytes_needed  | 0
   *                      | envp[...]
	 *                      | 0
	 *                      | argv[...]      <=== %rsp(8)
	 *                      + argc           <=== %rsp(0)
	 *
	 */
	constexpr size_t num_auxv_entries = 5;
	size_t argc, envc;
	const size_t data_bytes_needed = CalculateVectorStorageInBytes(argv, argc) + CalculateVectorStorageInBytes(envp, envc);
	const size_t record_bytes_needed =
	 (1 /* argc */ + argc + 1 /* terminating null */ + envc + 1 /* terminating null */) * sizeof(register_t) +
	 num_auxv_entries * sizeof(Elf_Auxv);
	const size_t padding_bytes_needed = (data_bytes_needed % 8 > 0) ? 8 - (data_bytes_needed % 8) : 0;
	const size_t bytes_needed = record_bytes_needed + data_bytes_needed + padding_bytes_needed;
	KASSERT(bytes_needed < PAGE_SIZE, "TODO deal with more >1 page here!");

	// Allocate a backing page for the userland stack and fill it
	{
		auto p = static_cast<char*>(kmem_map(stack_vp.GetPage()->GetPhysicalAddress(), PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE));
		memset(p, 0, PAGE_SIZE);
		{
			addr_t stack_ptr = reinterpret_cast<addr_t>(p) + PAGE_SIZE - bytes_needed;
			register_t stack_data_ptr = stack_end - data_bytes_needed - padding_bytes_needed;

			place(stack_ptr, static_cast<register_t>(argc));
			for (size_t n = 0; n < argc; n++) {
				place(stack_ptr, stack_data_ptr);
				stack_data_ptr += strlen(argv[n]) + 1 /* terminating \0 */;
			}
			place(stack_ptr, static_cast<register_t>(0));
			for (size_t n = 0; n < envc; n++) {
				place(stack_ptr, stack_data_ptr);
				stack_data_ptr += strlen(envp[n]) + 1 /* terminating \0 */;
			}
			place(stack_ptr, static_cast<register_t>(0));

			// Note: the amount of items here must correspond with num_auxv_entries !
			place(stack_ptr, Elf_Auxv{AT_ENTRY, static_cast<long>(auxargs->aa_entry)});
			place(stack_ptr, Elf_Auxv{AT_BASE, static_cast<long>(auxargs->aa_interpreter_base)});
			place(stack_ptr, Elf_Auxv{AT_PHDR, static_cast<long>(auxargs->aa_phdr)});
			place(stack_ptr, Elf_Auxv{AT_PHENT, static_cast<long>(auxargs->aa_phdr_entries)});
			place(stack_ptr, Elf_Auxv{AT_NULL, 0});

			for (size_t n = 0; n < argc; n++) {
				place(stack_ptr, *argv[n], strlen(argv[n]) + 1 /* terminating \0 */);
			}
			for (size_t n = 0; n < envc; n++) {
				place(stack_ptr, *envp[n], strlen(envp[n]) + 1 /* terminating \0 */);
			}
			place(stack_ptr, static_cast<register_t>(0), padding_bytes_needed); // padding

			KASSERT(stack_ptr == reinterpret_cast<addr_t>(p) + PAGE_SIZE, "did not fill entire userland stack; ptr %p != end %p\n", stack_ptr, reinterpret_cast<addr_t>(p) + PAGE_SIZE);
		}
		kmem_unmap(p, PAGE_SIZE);
	}

	md::thread::SetupPostExec(t, auxargs->aa_rip, stack_end - bytes_needed);

	stack_vp.Unlock();
	delete auxargs;
	return Result::Success();
}

EXECUTABLE_FORMAT("elf64", elf64Loader);

/* vim:set ts=2 sw=2: */
