/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
/*
 * XXX This file is an incredible hack - it will write a Linux-like coredump
 *     that is 'good enough' to use in GDB, but lldb is much more picky and
 *     will need extra patches.
 *
 * This needs serious cleanup, but at least the coredumps work and are thus
 * useful that I commit it here. If you are bored, feel free to clean this
 * up!
 */
#include <cstdint>
#include <ananas/elf.h>
#include <ananas/flags.h>
#include "kernel/pcpu.h"
#include "kernel/process.h"
#include "kernel/thread.h"
#include "kernel/result.h"
#include "kernel/vfs/types.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"

#include "kernel/vmarea.h"
#include "kernel/vmspace.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/vm.h"

static addr_t last_exec_addr = 0; // TODO

struct elf64_linux_siginfo {
    int32_t si_signo;
    int32_t si_code;
    int32_t si_errno;
};

// user_regs_struct
struct elf64_linux_regset {
    unsigned long long int r15;
    unsigned long long int r14;
    unsigned long long int r13;
    unsigned long long int r12;
    unsigned long long int rbp;
    unsigned long long int rbx;
    unsigned long long int r11;
    unsigned long long int r10;
    unsigned long long int r9;
    unsigned long long int r8;
    unsigned long long int rax;
    unsigned long long int rcx;
    unsigned long long int rdx;
    unsigned long long int rsi;
    unsigned long long int rdi;
    unsigned long long int orig_rax;
    unsigned long long int rip;
    unsigned long long int cs;
    unsigned long long int eflags;
    unsigned long long int rsp;
    unsigned long long int ss;
    unsigned long long int fs_base;
    unsigned long long int gs_base;
    unsigned long long int ds;
    unsigned long long int es;
    unsigned long long int fs;
    unsigned long long int gs;
};

typedef uint32_t linux_pid_t;

struct linux_timeval {
    uint64_t tv_sec;
    uint64_t tv_usec;
};

struct elf64_linux_prstatus {
    struct elf64_linux_siginfo pr_siginfo;
    uint16_t pr_cursigno;
    alignas(8) uint64_t pr_sigpend;
    alignas(8) uint64_t pr_sighold;

    linux_pid_t pr_pid;
    linux_pid_t pr_ppid;
    linux_pid_t pr_pgrp;
    linux_pid_t pr_sid;
    struct linux_timeval pr_utime;
    struct linux_timeval pr_stime;
    struct linux_timeval pr_cutime;
    struct linux_timeval pr_cstime;

    elf64_linux_regset pr_reg;
    int pr_fpvalid;
};

struct elf64_linux_prpsinfo {
    char pr_state;
    char pr_sname;
    char pr_zombie;
    char pr_nice;
    alignas(8) uint64_t pr_flag;
    uint32_t pr_uid;
    uint32_t pr_gid;
    int32_t pr_pid;
    int32_t pr_ppid;
    int32_t pr_pgrp;
    int32_t pr_sid;
    char pr_fname[16];
    char pr_psargs[80];
};

struct gnu_abi_tag {
    uint32_t version_info[4];
};

struct ananas_abi_tag {
    uint32_t version;
};

//#define GNU_ABI_OS_LINUX 0x00

#define LINUX_NT_PRSTATUS 1
#define LINUX_NT_FPREGSET 2
#define LINUX_NT_PRPSINFO 3
#define LINUX_NT_AUXV 6
#define LINUX_NT_FILE 0x46494c45
#define LINUX_NT_SIGINFO 0x53494749
#define LINUX_NT_X86_XSTATE 0x202
#define GNU_NT_ABI_TAG 1

#define ANANAS_NT_ABI_TAG 1

#define AUXV_AT_ENTRY 9
#define AUXV_AT_NULL 0

void elf64_add_note(char*& cur, const char* name, size_t contentLen, uint32_t type)
{
    size_t nameLen = strlen(name) + 1 /* terminating \0 */;

    *(uint32_t*)cur = nameLen;
    cur += sizeof(uint32_t); // namesz
    *(uint32_t*)cur = contentLen;
    cur += sizeof(uint32_t); // descsz
    *(uint32_t*)cur = type;
    cur += sizeof(uint32_t); // type

    strcpy(cur, name);
    cur += nameLen; // name

    // Ensure 4 byte alignment
    if (nameLen & 3) {
        cur += 4 - (nameLen & 3);
    }
}

Result elf64_coredump(Thread* t, struct STACKFRAME* sf, struct VFS_FILE* f)
{
    auto& proc = t->t_process;
    auto& vs = proc.p_vmspace;

    /*
     * Create the NOTE section; this is the most important as is contains context
     * information (registers, dynamic libraries etc).
     */
    char* note = static_cast<char*>(kmalloc(1024 * 1024)); /* XXX */
    char* cur = note;

#if 0
	// Add a GNU ABI tag for Linux so that lldb knows to parse our CORE things
	// XXX I should add a specific Ananas type to lldb instead
	{
		elf64_add_note(cur, "GNU", sizeof(struct gnu_abi_tag), GNU_NT_ABI_TAG);

		auto abitag = reinterpret_cast<struct gnu_abi_tag*>(cur);
		cur += sizeof(*abitag);
		abitag->version_info[0] = GNU_ABI_OS_LINUX;
		abitag->version_info[1] = 0;
		abitag->version_info[2] = 0;
		abitag->version_info[3] = 0;
	}
#endif
    {
        elf64_add_note(cur, "Ananas", sizeof(struct ananas_abi_tag), ANANAS_NT_ABI_TAG);

        auto abitag = reinterpret_cast<struct ananas_abi_tag*>(cur);
        cur += sizeof(*abitag);

        abitag->version = 0;
    }

    {
        elf64_add_note(cur, "CORE", sizeof(struct elf64_linux_prpsinfo), LINUX_NT_PRPSINFO);

        auto prps = reinterpret_cast<struct elf64_linux_prpsinfo*>(cur);
        cur += sizeof(*prps);

        memset(prps, 0, sizeof(*prps));
        prps->pr_pid = proc.p_pid;
        strcpy(prps->pr_fname, "./m");
    }

    {
        uint64_t auxv[] = {AUXV_AT_ENTRY, last_exec_addr, AUXV_AT_NULL, 0};

        elf64_add_note(cur, "CORE", sizeof(auxv), LINUX_NT_AUXV);
        memcpy(cur, auxv, sizeof(auxv));
        cur += sizeof(auxv);
    }

    {
        uint8_t reg2[] = {
            0x7f, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x1f, 0x00, 0x00,
            0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        };

        elf64_add_note(cur, "CORE", sizeof(reg2), LINUX_NT_FPREGSET);
        memcpy(cur, reg2, sizeof(reg2));
        cur += sizeof(reg2);
    }

    {
        elf64_add_note(cur, "CORE", sizeof(struct elf64_linux_prstatus), LINUX_NT_PRSTATUS);

        auto prs = reinterpret_cast<struct elf64_linux_prstatus*>(cur);
        cur += sizeof(*prs);

        memset(prs, 0, sizeof(*prs));
        prs->pr_pid = proc.p_pid;
        prs->pr_reg.rax = sf->sf_rax;
        prs->pr_reg.rbx = sf->sf_rbx;
        prs->pr_reg.rcx = sf->sf_rcx;
        prs->pr_reg.rdx = sf->sf_rdx;
        prs->pr_reg.rbp = sf->sf_rbp;
        prs->pr_reg.rsi = sf->sf_rsi;
        prs->pr_reg.rdi = sf->sf_rdi;
        prs->pr_reg.r8 = sf->sf_r8;
        prs->pr_reg.r9 = sf->sf_r9;
        prs->pr_reg.r10 = sf->sf_r10;
        prs->pr_reg.r11 = sf->sf_r11;
        prs->pr_reg.r12 = sf->sf_r12;
        prs->pr_reg.r13 = sf->sf_r13;
        prs->pr_reg.r14 = sf->sf_r14;
        prs->pr_reg.r15 = sf->sf_r15;
        prs->pr_reg.ds = sf->sf_ds;
        prs->pr_reg.es = sf->sf_es;
        prs->pr_reg.rip = sf->sf_rip;
        prs->pr_reg.cs = sf->sf_cs;
        prs->pr_reg.eflags = sf->sf_rflags;
        prs->pr_reg.rsp = sf->sf_rsp;
        prs->pr_reg.ss = sf->sf_ss;
    }

    {
        elf64_add_note(cur, "CORE", sizeof(struct elf64_linux_siginfo), LINUX_NT_SIGINFO);

        auto si = reinterpret_cast<struct elf64_linux_siginfo*>(cur);
        cur += sizeof(*si);

        memset(si, 0, sizeof(*si));
        si->si_signo = 9;
    }

    // Count mapped files and length of names
    size_t file_count = 0;
    size_t total_len = 2 * 8;
    for (auto& [ interval, va ] : vs.vs_areamap) {
        if (va->va_dentry == nullptr)
            continue;

        size_t plen = dentry_construct_path(NULL, 0, *va->va_dentry);
        file_count++;
        total_len += 3 * 8 + plen + 1;
    }

    while (total_len & 3)
        total_len++;

    // mapped files
    elf64_add_note(cur, "CORE", total_len, LINUX_NT_FILE);

    *(uint64_t*)cur = file_count;
    cur += sizeof(uint64_t);
    *(uint64_t*)cur = PAGE_SIZE;
    cur += sizeof(uint64_t);

    uint64_t* p = reinterpret_cast<uint64_t*>(cur);
    for (auto& [ interval, va ] : vs.vs_areamap) {
        if (va->va_dentry == nullptr)
            continue;
        *p++ = va->va_virt;             // start

        *p++ = va->va_virt + va->va_len; // end
        if ((va->va_doffset % PAGE_SIZE) != 0)
            kprintf("UNALIGNED DOFFSET %x\n", (int)va->va_doffset);
        *p++ = va->va_doffset / PAGE_SIZE; // offset
    }
    cur = reinterpret_cast<char*>(p);
    for (auto& [ interval, va ] : vs.vs_areamap) {
        if (va->va_dentry == nullptr)
            continue;
        size_t plen = dentry_construct_path(cur, 1024 /* XXX */, *va->va_dentry);
        cur += plen;
        *cur++ = '\0';
    }
    while ((addr_t)cur & 3)
        *cur++ = '\0';

    size_t note_len = cur - note;

    /*
     * Count the pages we will be writing to here; we'll write whatever is
     * mapped. XXX We could go for a more space-efficient manner by guessing
     * what is zero, what can be merged etc.
     */
    size_t num_phent = 1; // PT_NOTE
    for (auto& [ interval, va ] : vs.vs_areamap) {
        for (auto vp : va->va_pages) {
            if (vp == nullptr) continue;
            num_phent++;
        }
    }

    // Create and write the header
    {
        Elf64_Ehdr ehdr;

        memset(&ehdr, 0, sizeof(ehdr));
        ehdr.e_ident[EI_MAG0] = ELFMAG0;
        ehdr.e_ident[EI_MAG1] = ELFMAG1;
        ehdr.e_ident[EI_MAG2] = ELFMAG2;
        ehdr.e_ident[EI_MAG3] = ELFMAG3;
        ehdr.e_ident[EI_CLASS] = ELFCLASS64;
        ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
        ehdr.e_type = ET_CORE;
        ehdr.e_machine = EM_X86_64;
        ehdr.e_version = EV_CURRENT;
        ehdr.e_phoff = sizeof(ehdr); // Directly after the header
        ehdr.e_ehsize = sizeof(ehdr);
        ehdr.e_phentsize = sizeof(Elf64_Phdr);
        ehdr.e_phnum = num_phent;

        if (auto result = vfs_write(f, &ehdr, sizeof(ehdr)); result.IsFailure())
            return result;
    }

    // Write NOTE phdr as the first program header
    {
        Elf64_Phdr phdr;

        memset(&phdr, 0, sizeof(phdr));
        phdr.p_type = PT_NOTE;
        phdr.p_offset = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr) * num_phent;
        phdr.p_vaddr = 0;
        phdr.p_offset = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr) * num_phent;
        phdr.p_filesz = note_len;
        phdr.p_memsz = note_len;
        phdr.p_flags = PF_R;

        if (auto result = vfs_write(f, &phdr, sizeof(phdr)); result.IsFailure())
            return result;
    }

    // Store the other program header
    addr_t ph_begin = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr) * num_phent + note_len;
    ph_begin = (ph_begin | (PAGE_SIZE - 1)) + 1;
    size_t ph_index = 0;
    for (auto& [ interval, va ] : vs.vs_areamap) {
        size_t page_index = 0;
        for(size_t page_index = 0; page_index < va->va_pages.size(); ++page_index) {
            auto vp = va->va_pages[page_index];
            if (vp == nullptr) continue;
            Elf64_Phdr phdr;

            memset(&phdr, 0, sizeof(phdr));
            phdr.p_type = PT_LOAD;
            phdr.p_offset = ph_begin + ph_index * PAGE_SIZE;
            phdr.p_vaddr = interval.begin + page_index * PAGE_SIZE;
            phdr.p_filesz = PAGE_SIZE;
            phdr.p_align = PAGE_SIZE;
            phdr.p_memsz = PAGE_SIZE;
            phdr.p_flags = 0;
            if (vp->vp_flags & vm::flag::Read)
                phdr.p_flags |= PF_R;
            if (vp->vp_flags & vm::flag::Write)
                phdr.p_flags |= PF_W;
            if (vp->vp_flags & vm::flag::Execute)
                phdr.p_flags |= PF_X;

            if (auto result = vfs_write(f, &phdr, sizeof(phdr)); result.IsFailure())
                return result;

            ++ph_index;
        }
    }

    // Write the note data
    if (auto result = vfs_write(f, note, note_len); result.IsFailure())
        return result;

    // Align to PAGE_SIZE
    if (f->f_offset & (PAGE_SIZE - 1)) {
        memset(note, 0, PAGE_SIZE);
        size_t note_slack = PAGE_SIZE - f->f_offset & (PAGE_SIZE - 1);
        if (auto result = vfs_write(f, note, note_slack); result.IsFailure())
            return result;
    }

    // Store the page content
    for (auto& [ interval, va ] : vs.vs_areamap) {
        size_t page_index = 0;
        for(size_t page_index = 0; page_index < va->va_pages.size(); ++page_index) {
            auto vp = va->va_pages[page_index];
            if (vp == nullptr) continue;
            if (auto result = vfs_write(f, reinterpret_cast<void*>(interval.begin + page_index * PAGE_SIZE), PAGE_SIZE);
                result.IsFailure())
                return result;
        }
    }

    // kfree(note); // XXX leaks on error

    return Result::Success();
}

Result core_dump(Thread* t, struct STACKFRAME* sf)
{
    Process& proc = t->t_process;

    char path[64];
    snprintf(path, sizeof(path), "/dump/core.%d", proc.p_pid);
    path[sizeof(path) - 1] = '\0';

    mode_t mode = 0600;
    kprintf("core_dump: writing coredump to '%s'\n", path);

    struct VFS_FILE f;
    if (auto result = vfs_create(proc.p_cwd, path, O_RDWR, mode, &f); result.IsFailure())
        return result;

    if (auto result = elf64_coredump(t, sf, &f); result.IsFailure())
        return result;

    kprintf("core_dump: pre-close: offs=%d\n", (int)f.f_offset);

    return vfs_close(&proc, &f);
}
