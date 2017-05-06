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

struct ELF_THREADMAP_PROGHEADER;

/*
 * Describes a mapping from a portion of a file to a chunk of memory;
 * [ ph_virt_begin .. ph_virt_end ] is the range of memory mapped,
 * ph_inode_offset is the offset to read, up to ph_inode_len bytes
 *
 * Note that ph_inode_len can be < (ph_virt_end - ph_virt_begin); in
 * such a case, the remaining memory should be filled with 0-bytes.
 */
struct ELF_THREADMAP_PROGHEADER {
	struct ELF_THREADMAP_PRIVDATA* ph_header;
	addr_t         ph_virt_begin;
	addr_t         ph_virt_end;
	size_t         ph_inode_len;
	off_t          ph_inode_offset;
};

struct ELF_THREADMAP_PRIVDATA {
	struct DENTRY*	elf_dentry;
	int            	elf_num_refs;
	int            	elf_num_ph;
	struct ELF_THREADMAP_PROGHEADER elf_ph[1];
};

static errorcode_t
read_data(struct DENTRY* dentry, void* buf, off_t offset, size_t len)
{
	struct VFS_FILE f;
	memset(&f, 0, sizeof(f));
	f.f_dentry = dentry;

	/* kludge: cleanup is handeled by requesting 0 bytes at 0 to buffer NULL */
	if (buf == NULL && offset == 0 && len == 0) {
		dentry_deref(dentry);
		return ananas_success();
	}

	errorcode_t err = vfs_seek(&f, offset);
	ANANAS_ERROR_RETURN(err);

	size_t amount = len;
	err = vfs_read(&f, buf, &amount);
	ANANAS_ERROR_RETURN(err);

	if (amount != len)
		return ANANAS_ERROR(SHORT_READ);
	return ananas_success();
}

static errorcode_t
elf_tm_destroy_func(vmspace_t* vs, vmarea_t* va)
{
	auto ph = static_cast<struct ELF_THREADMAP_PROGHEADER*>(va->va_privdata);
	struct ELF_THREADMAP_PRIVDATA* privdata = ph->ph_header;

	if (--privdata->elf_num_refs == 0) {
		/* Last ref is gone; we can destroy the privdata now */
		read_data(privdata->elf_dentry, NULL, 0, 0); /* cleanup call */
		kfree(privdata);
	}
	return ananas_success();
}

static errorcode_t
elf_tm_fault_func(vmspace_t* vs, vmarea_t* va, addr_t virt)
{
	auto ph = static_cast<struct ELF_THREADMAP_PROGHEADER*>(va->va_privdata);
	struct ELF_THREADMAP_PRIVDATA* privdata = ph->ph_header;

	KASSERT((virt >= ph->ph_virt_begin && virt < ph->ph_virt_end), "wrong ph supplied (%p not in %p-%p)", virt, ph->ph_virt_begin, ph->ph_virt_end);

	/*
	 * Calculate what to read from where; we must fault per page, so we need to
	 * make sure the entire page is sane upon return.
	 */
	addr_t v_page = virt & ~(PAGE_SIZE - 1);
	addr_t read_addr = v_page;
	off_t read_off = ph->ph_inode_offset;
	if (read_addr >= ph->ph_virt_begin) {
		read_off += read_addr - ph->ph_virt_begin;
	} else /* read_addr < ph->ph_virt_begin */ {
		/* Page starts at an offset which isn't backed */
		read_addr = ph->ph_virt_begin;
	}

	/* And determine how much to read */
	size_t read_len = PAGE_SIZE;
	if (read_off > ph->ph_inode_offset + ph->ph_inode_len)
		read_len = 0; /* already past inode length; don't try to read anything */
	else if (read_off + read_len > ph->ph_inode_offset + ph->ph_inode_len) {
		/* We'd read beyond the length of the file - don't do that */
		read_len = (ph->ph_inode_offset + ph->ph_inode_len) - read_off;
	}
	TRACE(EXEC, INFO, "ph: va=%p, v=%p, reading %d bytes @ %d to %p (zeroing %p-%p)",
	 va, virt, read_len, (uint32_t)read_off, read_addr, v_page, v_page + PAGE_SIZE - 1);
	if (read_addr != v_page || read_len != PAGE_SIZE)
		memset((void*)v_page, 0, PAGE_SIZE);
	if (read_len > 0)
		return read_data(privdata->elf_dentry, (void*)read_addr, read_off, read_len);
	return ananas_success();
}

static errorcode_t
elf_tm_clone_func(vmspace_t* vs_src, vmarea_t* va_src, vmspace_t* vs_dst, vmarea_t* va_dst)
{
	/* We can just re-use the mapping; we add a ref to ensure it will not go away */
	va_dst->va_privdata = va_src->va_privdata;
	((struct ELF_THREADMAP_PROGHEADER*)va_dst->va_privdata)->ph_header->elf_num_refs++;
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

	/* Note that we allocate worst-case; there can be no more than ehdr.e_phnum sections */
	auto privdata = static_cast<struct ELF_THREADMAP_PRIVDATA*>(kmalloc(sizeof(struct ELF_THREADMAP_PRIVDATA) + sizeof(struct ELF_THREADMAP_PROGHEADER) * (ehdr.e_phnum - 1)));
	privdata->elf_dentry = dentry;
	privdata->elf_num_ph = 0;
	privdata->elf_num_refs = 0;

	for (unsigned int i = 0; i < ehdr.e_phnum; i++) {
		Elf64_Phdr phdr;
		err = read_data(dentry, &phdr, ehdr.e_phoff + i * ehdr.e_phentsize, sizeof(phdr));
		if (ananas_is_failure(err))
			goto fail;
		if (phdr.p_type != PT_LOAD)
			continue;

		/* Construct the flags for the actual mapping */
		unsigned int flags = VM_FLAG_ALLOC | VM_FLAG_LAZY | VM_FLAG_USER;
		flags |= VM_FLAG_READ; /* XXX */
		flags |= VM_FLAG_WRITE; /* XXX */
		flags |= VM_FLAG_EXECUTE; /* XXX */

		/*
		 * The program need not begin at a page-size, so we may need to adjust.
		 */
		addr_t virt_begin = ROUND_DOWN(phdr.p_vaddr, PAGE_SIZE);
		addr_t virt_end   = ROUND_UP((phdr.p_vaddr + phdr.p_memsz), PAGE_SIZE);
		vmarea_t* va;
		err = vmspace_mapto(vs, virt_begin, (addr_t)NULL, virt_end - virt_begin, flags, &va);
		if (ananas_is_failure(err))
			goto fail;

		/* Hook up the program header */
		struct ELF_THREADMAP_PROGHEADER* ph = &privdata->elf_ph[privdata->elf_num_ph];
		ph->ph_header = privdata;
		ph->ph_virt_begin = phdr.p_vaddr;
		ph->ph_virt_end = phdr.p_vaddr + phdr.p_memsz;
		ph->ph_inode_offset = phdr.p_offset;
		ph->ph_inode_len = phdr.p_filesz;

		privdata->elf_num_ph++;

		/* Hook the program header to the mapping */
		va->va_privdata = ph;
		va->va_fault = elf_tm_fault_func;
		va->va_destroy = elf_tm_destroy_func;
		va->va_clone = elf_tm_clone_func;
		privdata->elf_num_refs++;
	}

	*exec_addr = ehdr.e_entry;
	return ananas_success();

fail:
	kfree(privdata);
	return err;
}

EXECUTABLE_FORMAT("elf64", elf64_load);
#endif /* __amd64__ */

/* vim:set ts=2 sw=2: */
