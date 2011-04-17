#include <ananas/types.h>
#include <machine/thread.h>
#include <machine/param.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/thread.h>
#include <ananas/trace.h>
#include <ananas/vfs.h>
#include <ananas/mm.h>
#include <elf.h>

TRACE_SETUP;

/* XXX */
#ifdef _ARCH_PPC
#define __PowerPC__
#endif

struct ELF_THREADMAP_PROGHEADER;

struct ELF_THREADMAP_PROGHEADER {
	struct ELF_THREADMAP_PRIVDATA* ph_header;
	addr_t         ph_virt_begin;
	addr_t         ph_virt_end;
	size_t         ph_inode_len;
	off_t          ph_inode_delta;
	off_t          ph_inode_offset;
};

struct ELF_THREADMAP_PRIVDATA {
	elf_getfunc_t  elf_obtainfunc;
	void*          elf_obtainpriv;
	int            elf_num_refs;
	int            elf_num_ph;
	struct ELF_THREADMAP_PROGHEADER elf_ph[1];
};

static errorcode_t
elf_tm_destroy_func(thread_t t, struct THREAD_MAPPING* tm)
{
	struct ELF_THREADMAP_PROGHEADER* ph = tm->tm_privdata;
	struct ELF_THREADMAP_PRIVDATA* privdata = ph->ph_header;

	if (--privdata->elf_num_refs == 0) {
		/* Last ref is gone; we can destroy the privdata now */
		privdata->elf_obtainfunc(privdata->elf_obtainpriv, NULL, 0, 0); /* cleanup call */
		kfree(privdata);
	}
	return ANANAS_ERROR_OK;
}

static errorcode_t
elf_tm_fault_func(thread_t t, struct THREAD_MAPPING* tm, addr_t virt)
{
	struct ELF_THREADMAP_PROGHEADER* ph = tm->tm_privdata;
	struct ELF_THREADMAP_PRIVDATA* privdata = ph->ph_header;

	KASSERT((virt >= ph->ph_virt_begin && (virt + PAGE_SIZE) <= ph->ph_virt_end), "wrong ph supplied");
	
	/*
	 * Find the length and offset we have to transfer; the section doesn't have
	 * to be completely in the inode (parts that are zero aren't)
	 */
	off_t read_off = (virt - ph->ph_virt_begin) - ph->ph_inode_delta;
	size_t read_len = PAGE_SIZE;
	if (read_off > ph->ph_inode_len)
		read_off = ph->ph_inode_len;
	if (read_off + read_len > ph->ph_inode_len)
		read_len = ph->ph_inode_len - read_off;
	read_off += ph->ph_inode_offset;
	TRACE(EXEC, INFO, "ph: t=%p, loading page to 0x%p, file offset is %u, length is %u",
	 t, virt, (uint32_t)read_off, read_len);
	memset((void*)virt, 0, PAGE_SIZE);
	if (read_len > 0)
		return privdata->elf_obtainfunc(privdata->elf_obtainpriv, (void*)virt, read_off, read_len);
	return ANANAS_ERROR_NONE;
}

static errorcode_t
elf_tm_clone_func(thread_t t, struct THREAD_MAPPING* tdest, struct THREAD_MAPPING* tsrc)
{
	/* We can just re-use the mapping; we add a ref to ensure it will not go away */
	tdest->tm_privdata = tsrc->tm_privdata;
	((struct ELF_THREADMAP_PROGHEADER*)tdest->tm_privdata)->ph_header->elf_num_refs++;
	return ANANAS_ERROR_NONE;
}

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

#ifdef LITTLE_ENDIAN
	if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB)
		return ANANAS_ERROR(BAD_EXEC);
#endif
#ifdef BIG_ENDIAN
	if (ehdr.e_ident[EI_DATA] != ELFDATA2MSB)
		return ANANAS_ERROR(BAD_EXEC);
#endif

	struct ELF_THREADMAP_PROGHEADER elf_ph[1];
#ifdef __i386__
	if (ehdr.e_machine != EM_386)
		return ANANAS_ERROR(BAD_EXEC);
#endif

	/* We only support static binaries for now, so reject anything without a program table */
	if (ehdr.e_phnum == 0)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr.e_phentsize < sizeof(Elf32_Phdr))
		return ANANAS_ERROR(BAD_EXEC);

	/* Note that we allocate worst-case; there can be no more than ehdr.e_phnum sections */
	struct ELF_THREADMAP_PRIVDATA* privdata = kmalloc(sizeof(struct ELF_THREADMAP_PRIVDATA) + sizeof(struct ELF_THREADMAP_PROGHEADER) * (ehdr.e_phnum - 1));
	privdata->elf_obtainpriv = priv;
	privdata->elf_obtainfunc = obtain;
	privdata->elf_num_ph = 0;
	privdata->elf_num_refs = 0;

	TRACE(EXEC, INFO, "found %u program headers", ehdr.e_phnum);
	for (unsigned int i = 0; i < ehdr.e_phnum; i++) {
		Elf32_Phdr phdr;
		TRACE(EXEC, INFO, "ph %u: obtaining header from offset %u", i, ehdr.e_phoff + i * ehdr.e_phentsize);
		err = obtain(priv, &phdr, ehdr.e_phoff + i * ehdr.e_phentsize, sizeof(phdr));
		if (err != ANANAS_ERROR_NONE) {
			TRACE(EXEC, INFO, "ph %u: obtain failed: %i", i, err);
			goto fail;
		}
		TRACE(EXEC, INFO, "ph %u: obtained, header type=%u", i, phdr.p_type);
		if (phdr.p_type != PT_LOAD)
			continue;

		/* Construct the flags for the actual mapping */
		unsigned int flags = THREAD_MAP_ALLOC | THREAD_MAP_LAZY;
		flags |= THREAD_MAP_READ; /* XXX */
		flags |= THREAD_MAP_WRITE; /* XXX */
		flags |= THREAD_MAP_EXECUTE; /* XXX */

		/*
		 * The program need not begin at a page-size, so we may need to adjust.
		 */
		addr_t virt_begin = ROUND_DOWN(phdr.p_vaddr, PAGE_SIZE);
		addr_t virt_end   = ROUND_UP((phdr.p_vaddr + phdr.p_memsz), PAGE_SIZE);
		TRACE(EXEC, INFO, "ph %u: instantiated mapping for %x-%x",
		 privdata->elf_num_ph, virt_begin, virt_end);
		struct THREAD_MAPPING* tm;
		err = thread_mapto(thread, virt_begin, (addr_t)NULL, virt_end - virt_begin, flags, &tm);
		if (err != ANANAS_ERROR_OK)
			goto fail;

		/* Hook up the program header */
		struct ELF_THREADMAP_PROGHEADER* ph = &privdata->elf_ph[privdata->elf_num_ph];
		ph->ph_header = privdata;
		ph->ph_virt_begin = virt_begin;
		ph->ph_virt_end = virt_end;
		ph->ph_inode_delta = phdr.p_offset % PAGE_SIZE;
		ph->ph_inode_offset = phdr.p_offset;
		ph->ph_inode_len = phdr.p_filesz;
		privdata->elf_num_ph++;

		/* Hook the program header to the mapping */
		tm->tm_privdata = ph;
		tm->tm_fault = elf_tm_fault_func;
		tm->tm_destroy = elf_tm_destroy_func;
		tm->tm_clone = elf_tm_clone_func;
		privdata->elf_num_refs++;
	}
	TRACE(EXEC, INFO, "done, entry point is 0x%x", ehdr.e_entry);
	md_thread_set_entrypoint(thread, ehdr.e_entry);

	return ANANAS_ERROR_OK;

fail:
	kfree(privdata);
	return err;
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
	struct ELF_THREADMAP_PRIVDATA* privdata = kmalloc(sizeof(struct ELF_THREADMAP_PRIVDATA) + sizeof(struct ELF_THREADMAP_PROGHEADER) * (ehdr.e_phnum - 1));
	privdata->elf_obtainpriv = priv;
	privdata->elf_obtainfunc = obtain;
	privdata->elf_num_ph = 0;
	privdata->elf_num_refs = 0;

	for (unsigned int i = 0; i < ehdr.e_phnum; i++) {
		Elf64_Phdr phdr;
		err = obtain(priv, &phdr, ehdr.e_phoff + i * ehdr.e_phentsize, sizeof(phdr));
		if (err != ANANAS_ERROR_NONE)
			goto fail;
		if (phdr.p_type != PT_LOAD)
			continue;

		/* Construct the flags for the actual mapping */
		unsigned int flags = THREAD_MAP_ALLOC | THREAD_MAP_LAZY;
		flags |= THREAD_MAP_READ; /* XXX */
		flags |= THREAD_MAP_WRITE; /* XXX */
		flags |= THREAD_MAP_EXECUTE; /* XXX */

		/*
		 * The program need not begin at a page-size, so we may need to adjust.
		 */
		addr_t virt_begin = ROUND_DOWN(phdr.p_vaddr, PAGE_SIZE);
		addr_t virt_end   = ROUND_UP((phdr.p_vaddr + phdr.p_memsz), PAGE_SIZE);
		struct THREAD_MAPPING* tm;
		err = thread_mapto(thread, virt_begin, (addr_t)NULL, virt_end - virt_begin, flags, &tm);
		if (err != ANANAS_ERROR_OK)
			goto fail;

		/* Hook up the program header */
		struct ELF_THREADMAP_PROGHEADER* ph = &privdata->elf_ph[privdata->elf_num_ph];
		ph->ph_header = privdata;
		ph->ph_virt_begin = virt_begin;
		ph->ph_virt_end = virt_end;
		ph->ph_inode_delta = phdr.p_offset % PAGE_SIZE;
		ph->ph_inode_offset = phdr.p_offset;
		ph->ph_inode_len = phdr.p_filesz;
		privdata->elf_num_ph++;

		/* Hook the program header to the mapping */
		tm->tm_privdata = ph;
		tm->tm_fault = elf_tm_fault_func;
		tm->tm_destroy = elf_tm_destroy_func;
		tm->tm_clone = elf_tm_clone_func;
		privdata->elf_num_refs++;
	}

	md_thread_set_entrypoint(thread, ehdr.e_entry);
	return ANANAS_ERROR_OK;

fail:
	kfree(privdata);
	return err;
}
#endif /* __amd64__ */

static errorcode_t
elf_load_inodefunc(void* priv, void* buf, off_t offset, size_t len)
{
	struct VFS_FILE f;
	memset(&f, 0, sizeof(f));
	f.f_inode = priv;

	/* XXX Hack: cleanup is handeled by requesting 0 bytes at 0 to buffer NULL */
	if (buf == NULL && offset == 0 && len == 0) {
		vfs_deref_inode(priv);
		return ANANAS_ERROR_OK;
	}

	errorcode_t err = vfs_seek(&f, offset);
	ANANAS_ERROR_RETURN(err);

	size_t amount = len;
	err = vfs_read(&f, buf, &amount);
	ANANAS_ERROR_RETURN(err);

	if (amount != len)
		return ANANAS_ERROR(SHORT_READ);
	return ANANAS_ERROR_OK;
}

errorcode_t
elf_load_from_file(thread_t t, struct VFS_INODE* inode)
{
	/* Ensure the inode will not go away */
	vfs_ref_inode(inode);
	errorcode_t err = 
#if defined(__i386__ ) || defined(__PowerPC__)
		elf32_load(t, inode, elf_load_inodefunc)
#elif defined(__amd64__)
		elf64_load(t, inode, elf_load_inodefunc)
#else
		ANANAS_ERROR(BAD_EXEC)
#endif
	;
	if(err != ANANAS_ERROR_NONE)
		vfs_deref_inode(inode);
	return err;
}

/* vim:set ts=2 sw=2: */
