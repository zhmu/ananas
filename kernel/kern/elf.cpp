/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <machine/param.h>
#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/elf.h>
#include <ananas/flags.h>
#include "kernel/exec.h"
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/process.h"
#include "kernel/thread.h"
#include "kernel/userland.h"
#include "kernel/vmarea.h"
#include "kernel/vmspace.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/types.h"
#include "kernel/vm.h"
#include "kernel-md/md.h"
#include "kernel-md/param.h"

// XXX The next constants are a kludge - we need to have a nicer mechanism to allocate
//     virtual address space (vmspace should be extended for dynamic mappings)
namespace {
    inline constexpr addr_t INTERPRETER_BASE = 0xf000000;
    inline constexpr addr_t PHDR_BASE = 0xd0000000;
}

namespace
{
    Result read_data(DEntry& dentry, void* buf, off_t offset, size_t len)
    {
        struct VFS_FILE f;
        memset(&f, 0, sizeof(f));
        f.f_dentry = &dentry;

        if (auto result = vfs_seek(&f, offset); result.IsFailure())
            return result;

        Result result = vfs_read(&f, buf, len);
        if (result.IsFailure())
            return result;

        size_t numread = result.AsValue();
        if (numread != len)
            return Result::Failure(EIO);
        return Result::Success();
    }

    Result LoadEhdr(DEntry& dentry, Elf64_Ehdr& ehdr)
    {
        if (auto result = read_data(dentry, &ehdr, 0, sizeof(ehdr)); result.IsFailure())
            return result;

        // Perform basic ELF checks: check magic/version
        if (ehdr.e_ident[EI_MAG0] != ELFMAG0 || ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
            ehdr.e_ident[EI_MAG2] != ELFMAG2 || ehdr.e_ident[EI_MAG3] != ELFMAG3)
            return Result::Failure(ENOEXEC);
        if (ehdr.e_ident[EI_VERSION] != EV_CURRENT)
            return Result::Failure(ENOEXEC);

        // XXX This specifically checks for amd64 at the moment
        if (ehdr.e_ident[EI_CLASS] != ELFCLASS64)
            return Result::Failure(ENOEXEC);
        if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB)
            return Result::Failure(ENOEXEC);
        if (ehdr.e_machine != EM_X86_64)
            return Result::Failure(ENOEXEC);

        // There must be something to load
        if (ehdr.e_phnum == 0)
            return Result::Failure(ENOEXEC);
        if (ehdr.e_phentsize < sizeof(Elf64_Phdr))
            return Result::Failure(ENOEXEC);

        return Result::Success();
    }

    struct AuxArgs {
        addr_t aa_interpreter_base = 0;
        addr_t aa_phdr = 0;
        size_t aa_phdr_entries = 0;
        addr_t aa_entry = 0;
        addr_t aa_rip = 0;
    };

    Result LoadPH(VMSpace& vs, DEntry& dentry, const Elf64_Phdr& phdr, addr_t rbase)
    {
        /* Construct the flags for the actual mapping */
        unsigned int flags = vm::flag::User;
        if (phdr.p_flags & PF_R)
            flags |= vm::flag::Read;
        if (phdr.p_flags & PF_W)
            flags |= vm::flag::Write;
        if (phdr.p_flags & PF_X)
            flags |= vm::flag::Execute;

        // If this mapping is writable, do not share it - don't want changes to end
        // up in the inode
        if (flags & vm::flag::Write)
            flags |= vm::flag::Private;

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
        addr_t virt_end = ROUND_UP((phdr.p_vaddr + phdr.p_filesz), PAGE_SIZE);
        addr_t virt_extra = phdr.p_vaddr - virt_begin;
        off_t doffset = phdr.p_offset - virt_extra;
        off_t filesz = phdr.p_filesz + virt_extra;
        if (doffset & (PAGE_SIZE - 1)) {
            kprintf("elf64_load_ph: refusing to map offset %d not a page-multiple\n", doffset);
            return Result::Failure(ENOEXEC);
        }

        // First step is to map the dentry-backed data
        VMArea* va;
        if (auto result = vs.MapToDentry(
                VAInterval{ rbase + virt_begin, rbase + virt_end }, dentry,
                DEntryInterval{ doffset, doffset + filesz }, flags, va);
            result.IsFailure())
            return result;
        if (phdr.p_filesz == phdr.p_memsz)
            return Result::Success();

        // Now map the part that isn't dentry-backed, if we have enough
        addr_t v_extra = virt_end;
        addr_t v_extra_len = ROUND_UP(phdr.p_vaddr + phdr.p_memsz - v_extra, PAGE_SIZE);
        if (v_extra_len == 0)
            return Result::Success();
        return vs.MapTo(VAInterval{ rbase + v_extra, rbase + v_extra + v_extra_len }, flags, va);
    }

    Result LoadFile(VMSpace& vs, DEntry& dentry, const Elf64_Ehdr& ehdr, addr_t rbase, addr_t& exec_addr)
    {
        /* Process all program headers one by one */
        for (unsigned int i = 0; i < ehdr.e_phnum; i++) {
            Elf64_Phdr phdr;
            if (auto result =
                    read_data(dentry, &phdr, ehdr.e_phoff + i * ehdr.e_phentsize, sizeof(phdr));
                result.IsFailure())
                return result;
            if (phdr.p_type != PT_LOAD)
                continue;

            if (auto result = LoadPH(vs, dentry, phdr, rbase); result.IsFailure())
                return result;
        }

        exec_addr = ehdr.e_entry;
        return Result::Success();
    }

    Result LocateInterpreter(const Elf64_Ehdr& ehdr, DEntry& dentry, char*& interp)
    {
        for (unsigned int i = 0; i < ehdr.e_phnum; i++) {
            Elf64_Phdr phdr;
            if (auto result =
                    read_data(dentry, &phdr, ehdr.e_phoff + i * ehdr.e_phentsize, sizeof(phdr));
                result.IsFailure())
                return result;
            switch (phdr.p_type) {
                case PT_INTERP: {
                    if (interp != nullptr) {
                        kfree(interp);
                        kprintf("elf64_load: multiple interp headers\n");
                        return Result::Failure(ENOEXEC);
                    }

                    if (phdr.p_filesz >= PAGE_SIZE) {
                        kprintf("elf64_load: interp too large\n");
                        return Result::Failure(ENOEXEC);
                    }

                    // Read the interpreter from disk
                    interp = static_cast<char*>(kmalloc(phdr.p_filesz + 1));
                    interp[phdr.p_filesz] = '\0';
                    if (auto result = read_data(dentry, interp, phdr.p_offset, phdr.p_filesz);
                        result.IsFailure()) {
                        kfree(interp);
                        return result;
                    }
                }
            }
        }

        return Result::Success();
    }

    Result LoadInterpreter(VMSpace& vs, const Elf64_Ehdr& ehdr, DEntry& dentry, const char* interp, AuxArgs* auxargs)
    {
        // We need to use an interpreter to load this
        struct VFS_FILE interp_file;
        // Note that the nullptr requires interp to be absolute
        if (auto result = vfs_open(nullptr, interp, nullptr, O_RDONLY, VFS_LOOKUP_FLAG_DEFAULT, &interp_file); result.IsFailure()) {
            kprintf("unable to load ELF interpreter '%s': %d\n", interp, result.AsStatusCode());
            return result;
        }

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
            auto result =
                LoadFile(vs, *interp_file.f_dentry, interp_ehdr, interp_rbase, interp_entry);
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
            const off_t phdrOffset = ehdr.e_phoff & ~(PAGE_SIZE - 1);
            const off_t phdr_len = ehdr.e_phnum * ehdr.e_phentsize;
            if (auto result = vs.MapToDentry(
                    VAInterval{ PHDR_BASE, PHDR_BASE + phdr_len },
                    dentry,
                    DEntryInterval{ phdrOffset, phdrOffset + phdr_len },
                    vm::flag::Read | vm::flag::User, va_phdr);
                result.IsFailure())
                return result;
        }

        // Add auxiliary arguments for dynamic executables
        auxargs->aa_interpreter_base = interp_rbase;
        auxargs->aa_phdr = va_phdr->va_virt + (ehdr.e_phoff & (PAGE_SIZE - 1));
        auxargs->aa_phdr_entries = ehdr.e_phnum;

        // Invoke the interpreter instead of the executable; it can use AT_ENTRY to
        // determine the executable entry point
        auxargs->aa_rip = interp_entry + interp_rbase;
        return Result::Success();
    }
} // unnamed namespace

struct ELF64Loader final : IExecutor {
    Result Verify(DEntry& dentry) override;
    Result Load(VMSpace& vs, DEntry& dentry, void*& auxargs) override;
    Result PrepareForExecute(
        VMSpace& vs, Thread& t, void* auxargs, const char* argv[], const char* envp[]) override;
};

Result ELF64Loader::Verify(DEntry& dentry)
{
    // Fetch the ELF header (again - this could be nicer...)
    Elf64_Ehdr ehdr;
    if (auto result = LoadEhdr(dentry, ehdr); result.IsFailure())
        return result;

    // Only accept executables here
    if (ehdr.e_type != ET_EXEC)
        return Result::Failure(ENOEXEC);

    return Result::Success();
}

Result ELF64Loader::Load(VMSpace& vs, DEntry& dentry, void*& aa)
{
    // Fetch the ELF header (again - this could be nicer...)
    Elf64_Ehdr ehdr;
    if (auto result = LoadEhdr(dentry, ehdr); result.IsFailure())
        return result;

    // Load the file
    addr_t exec_addr;
    if (auto result = LoadFile(vs, dentry, ehdr, 0, exec_addr); result.IsFailure())
        return result;

    // This went okay - see if there any other interesting headers here
    char* interp = nullptr;
    if (auto result = LocateInterpreter(ehdr, dentry, interp); result.IsFailure())
        return result;

    // Initialize auxiliary arguments; these will be passed using the auxv mechanism
    auto auxargs = new AuxArgs;
    auxargs->aa_entry = exec_addr;

    auto result = Result::Success();
    if (interp != nullptr) {
        result = LoadInterpreter(vs, ehdr, dentry, interp, auxargs);
        kfree(interp);
    } else {
        // Directly invoke the static executable; no interpreter present
        auxargs->aa_rip = exec_addr;
    }

    aa = static_cast<void*>(auxargs);
    return result;
}

Result ELF64Loader::PrepareForExecute(
    VMSpace& vs, Thread& t, void* aa, const char* argv[], const char* envp[])
{
    auto auxargs = static_cast<AuxArgs*>(aa);

    const auto stackEnd = USERLAND_STACK_ADDR + THREAD_STACK_SIZE;
    auto& stack_vp = userland::CreateStack(vs, stackEnd);

    /*
     * Calculate the amount of space we need; the ELF ABI specifies the stack to
     * be laid out as follows:
     *
     * padding_bytes_needed + 0              ends at stackEnd
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
    const size_t data_bytes_needed =
        userland::CalculateVectorStorageInBytes(argv, argc) +
        userland::CalculateVectorStorageInBytes(envp, envc);
    const size_t record_bytes_needed =
        (1 /* argc */ + argc + 1 /* terminating null */ + envc + 1 /* terminating null */) *
            sizeof(register_t) +
        num_auxv_entries * sizeof(Elf_Auxv);
    const size_t padding_bytes_needed = userland::CalculatePaddingBytesNeeded(data_bytes_needed);
    const size_t bytes_needed = record_bytes_needed + data_bytes_needed + padding_bytes_needed;
    KASSERT(bytes_needed < PAGE_SIZE, "TODO deal with more >1 page here!");

    // Map the last page of userland stack and fill it
    {
        auto p = static_cast<char*>(kmem_map(
            stack_vp.GetPage()->GetPhysicalAddress(), PAGE_SIZE, vm::flag::Read | vm::flag::Write));
        memset(p, 0, PAGE_SIZE);
        {
            using userland::Store;
            addr_t stack_ptr = reinterpret_cast<addr_t>(p) + PAGE_SIZE - bytes_needed;
            register_t stack_data_ptr = stackEnd - data_bytes_needed - padding_bytes_needed;

            Store(stack_ptr, static_cast<register_t>(argc));
            for (size_t n = 0; n < argc; n++) {
                Store(stack_ptr, stack_data_ptr);
                stack_data_ptr += strlen(argv[n]) + 1 /* terminating \0 */;
            }
            Store(stack_ptr, static_cast<register_t>(0));
            for (size_t n = 0; n < envc; n++) {
                Store(stack_ptr, stack_data_ptr);
                stack_data_ptr += strlen(envp[n]) + 1 /* terminating \0 */;
            }
            Store(stack_ptr, static_cast<register_t>(0));

            // Note: the amount of items here must correspond with num_auxv_entries !
            Store(stack_ptr, Elf_Auxv{AT_ENTRY, static_cast<long>(auxargs->aa_entry)});
            Store(stack_ptr, Elf_Auxv{AT_BASE, static_cast<long>(auxargs->aa_interpreter_base)});
            Store(stack_ptr, Elf_Auxv{AT_PHDR, static_cast<long>(auxargs->aa_phdr)});
            Store(stack_ptr, Elf_Auxv{AT_PHENT, static_cast<long>(auxargs->aa_phdr_entries)});
            Store(stack_ptr, Elf_Auxv{AT_NULL, 0});

            for (size_t n = 0; n < argc; n++) {
                Store(stack_ptr, *argv[n], strlen(argv[n]) + 1 /* terminating \0 */);
            }
            for (size_t n = 0; n < envc; n++) {
                Store(stack_ptr, *envp[n], strlen(envp[n]) + 1 /* terminating \0 */);
            }
            Store(stack_ptr, static_cast<register_t>(0), padding_bytes_needed); // padding

            KASSERT(
                stack_ptr == reinterpret_cast<addr_t>(p) + PAGE_SIZE,
                "did not fill entire userland stack; ptr %p != end %p\n", stack_ptr,
                reinterpret_cast<addr_t>(p) + PAGE_SIZE);
        }
        kmem_unmap(p, PAGE_SIZE);
    }

    t.SetName(argv[0]);
    md::thread::SetupPostExec(t, auxargs->aa_rip, stackEnd - bytes_needed);

    stack_vp.Unlock();
    delete auxargs;
    return Result::Success();
}

static const RegisterExecutableFormat<ELF64Loader> registerELF64;
