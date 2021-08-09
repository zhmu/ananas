/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <loader/module.h>
#include <machine/param.h>
#include "kernel/cmdline.h"
#include "kernel/fd.h"
#include "kernel/init.h"
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/page.h"
#include "kernel/pcpu.h"
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/mm.h"
#include "kernel/vm.h"
#include "kernel/vmspace.h"
#include "kernel-md/acpi.h"
#include "kernel-md/macro.h"
#include "kernel-md/multiboot.h"
#include "kernel-md/interrupts.h"
#include "kernel-md/param.h"
#include "kernel-md/pic.h"
#include "kernel-md/pit.h"
#include "kernel-md/smp.h"
#include "kernel-md/thread.h"
#include "kernel-md/vm.h"

// Kernel virtual memory space
static char kernel_vmspace_storage[sizeof(VMSpace)];
VMSpace* kernel_vmspace = reinterpret_cast<VMSpace*>(&kernel_vmspace_storage);

/* Global Descriptor Table */
uint8_t bsp_gdt[GDT_NUM_ENTRIES * 16];

/* Interrupt Descriptor Table */
uint8_t idt[IDT_NUM_ENTRIES * 16];

/* Boot CPU pcpu structure */
static struct PCPU bsp_pcpu;

static TSS bsp_tss;

extern "C" void *__entry, *__end, *__rodata_end;
extern void* syscall_handler;
extern void* bootstrap_stack;

extern void* exception0;
extern void* exception1;
extern void* exception2;
extern void* exception3;
extern void* exception4;
extern void* exception5;
extern void* exception6;
extern void* exception7;
extern void* exception8;
extern void* exception9;
extern void* exception10;
extern void* exception11;
extern void* exception12;
extern void* exception13;
extern void* exception14;
extern void* exception15;
extern void* exception16;
extern void* exception17;
extern void* exception18;
extern void* exception19;
extern void* lapic_irq_range_1;
extern void* ipi_periodic;
extern void* ipi_panic;
extern void* irq_spurious;

Page* usupport_page;
void* usupport;

int video_resolution_width{};
int video_resolution_height{};
addr_t video_framebuffer{};

extern "C" void __run_global_ctors();

namespace
{
    void* bootstrap_get_pages(addr_t& avail, size_t num)
    {
        void* ptr = (void*)avail;
        memset(ptr, 0, num * PAGE_SIZE);
        avail += num * PAGE_SIZE;
        return ptr;
    }

    /*
     * Maps num_pages of phys -> virt; when pages are needed, *avail is used and incremented.
     *
     * get_flags(phys, virt) will be called for every actual mapping to determine the
     * mapping-specific flags that are to be used.
     */
    template<typename T>
    void
    map_kernel_pages(uint64_t* kernel_pagedir, addr_t phys, addr_t virt, unsigned int num_pages, addr_t& avail, T get_flags)
    {
        static constexpr uint64_t addr_mask = 0xffffffffff000; /* bits 12 .. 51 */

        for (unsigned int n = 0; n < num_pages; n++) {
            uint64_t* pml4e = &kernel_pagedir[(virt >> 39) & 0x1ff];
            if (*pml4e == 0) {
                *pml4e = avail | PE_RW | PE_P | PE_C_G;
                avail += PAGE_SIZE;
            }
            uint64_t* p = (uint64_t*)(*pml4e & addr_mask);
            uint64_t* pdpe = &p[(virt >> 30) & 0x1ff];
            if (*pdpe == 0) {
                *pdpe = avail | PE_RW | PE_P | PE_C_G;
                avail += PAGE_SIZE;
            }
            uint64_t* q = (uint64_t*)(*pdpe & addr_mask);
            uint64_t* pde = &q[(virt >> 21) & 0x1ff];
            if (*pde == 0) {
                *pde = avail | PE_RW | PE_P | PE_C_G;
                avail += PAGE_SIZE;
            }

            uint64_t* r = (uint64_t*)(*pde & addr_mask);
            r[(virt >> 12) & 0x1ff] = phys | get_flags(phys, virt);
            virt += PAGE_SIZE;
            phys += PAGE_SIZE;
        }
    }

    /*
     * Calculated how many PAGE_SIZE-sized pieces we need to map mem_size bytes - note that this is
     * only accurate if the memory is mapped at address zero (it doesn't consider crossing
     * boundaries)
     */
    void calculate_num_pages_required(
        size_t mem_size, unsigned int& num_pages_needed, unsigned int& length_in_pages)
    {
        /* Number of level-4 page-table entries required; these map bits 63..39 */
        unsigned int num_pml4e = (mem_size + (1ULL << 39) - 1) >> 39;
        /* Number of level-3 page-table entries required; these map bits 38..30 */
        unsigned int num_pdpe = (mem_size + (1ULL << 30) - 1) >> 30;
        /* Number of level-2 page-table entries required; these map bits 29..21 */
        unsigned int num_pde = (mem_size + (1ULL << 21) - 1) >> 21;
        /* Number of level-1 page-table entries required; these map bits 20..12 */
        unsigned int num_pte = (mem_size + (1ULL << 12) - 1) >> 12;
        num_pages_needed = num_pml4e + num_pdpe + num_pde;
        length_in_pages = num_pte;
    }

    auto setup_paging(addr_t& avail, addr_t mem_end, size_t kernel_size)
    {
        constexpr auto KMAP_KVA_START = KMEM_DIRECT_VA_START;
        constexpr auto KMAP_KVA_END = KMEM_DYNAMIC_VA_END;
        addr_t avail_start = avail;

        /*
         * Taking the overview in machine-md/vm.h into account, we want to map the
         * following regions:
         *
         * - KMAP_KVA_START .. KMAP_KVA_END: the kernel's KVA
         *   This may be mapped as 4KB pages; however, we can lower the estimate if
         *   there is less memory available than the total size of this region.
         * - KERNBASE ... KERNEND: the kernel code/data
         *   We always map this as 4KB pages to ensure we can benefit most optimally
         *   from NX.
         */
        uint64_t kmap_kva_end = KMAP_KVA_END;
        if (kmap_kva_end - KMAP_KVA_START >= mem_end) {
            /* Tone down KVA as we have less memory */
            kmap_kva_end = KMAP_KVA_START + mem_end;
        }

        // XXX Ensure we'll have at least 4GB KVA; this is needed in order to map PCI
        // devices (we should actually just add specific mappings for this place instead)
        if (mem_end < 4UL * 1024 * 1024 * 1024)
            kmap_kva_end = KMAP_KVA_START + 4UL * 1024 * 1024 * 1024;

        uint64_t kva_size = kmap_kva_end - KMAP_KVA_START;
        unsigned int kva_pages_needed, kva_size_in_pages;
        calculate_num_pages_required(kva_size, kva_pages_needed, kva_size_in_pages);
        addr_t kva_pages = (addr_t)bootstrap_get_pages(avail, kva_pages_needed);

        /* Finally, allocate the kernel pagedir itself */
        auto kernel_pagedir = static_cast<uint64_t*>(bootstrap_get_pages(avail, 1));
        kprintf(">>> kernel_pagedir = %p\n", kernel_pagedir);

        /*
         * Calculate the number of entries we need for the kernel itself; we just
         * need to grab the bare necessities as we'll map all other memory using the
         * KVA instead. We do this here to update the 'avail' pointer to ensure
         * we'll map these tables in KVA as well, should we ever need to change them.
         *
         * XXX We could consider mapping the kernel using 2MB pages if the NX bits align...
         */
        unsigned int kernel_pages_needed, kernel_size_in_pages;
        calculate_num_pages_required(kernel_size, kernel_pages_needed, kernel_size_in_pages);
        /*
         * XXX calculate_num_pages_required() doesn't consider page boundaries -
         * however, the kernel is generally mapped to address 1MB, which means we may
         * do a cross-over from address 1MB -> 2MB, which needs an extra PDE, which
         * we'll bluntly add here (maybe calculate_num_pages_required() needs to be more
         * intelligent... ?)
         */
        kernel_pages_needed++;
        addr_t kernel_pages = (addr_t)bootstrap_get_pages(avail, kernel_pages_needed);

        /*
         * Calculate the amount we need for dynamic KVA mappings; this is likely a
         * gross overstatement because we can typically map everything 1:1 since we
         * have address space plenty...
         */
        unsigned int dyn_kva_pages_needed, dyn_kva_size_in_pages;
        calculate_num_pages_required(
            KMEM_DYNAMIC_VA_END - KMEM_DYNAMIC_VA_START, dyn_kva_pages_needed,
            dyn_kva_size_in_pages);
        addr_t dyn_kva_pages = (addr_t)bootstrap_get_pages(avail, dyn_kva_pages_needed);

        /*
         * Map the KVA - we will only map what we have used for our page tables, to
         * ensure we can change them later as necessary. We explicitly won't map the
         * kernel page tables here because we never need to change them.
         */
        addr_t kva_avail_ptr = (addr_t)kva_pages;
        map_kernel_pages(
            kernel_pagedir,
            0, KMAP_KVA_START, kva_size_in_pages, kva_avail_ptr, [&](addr_t phys, addr_t virt) {
                uint64_t flags = 0;
                if (phys >= avail_start && phys <= avail)
                    flags = PE_G | PE_RW | PE_P;
                return flags;
            });
        KASSERT(
            kva_avail_ptr == (addr_t)kva_pages + kva_pages_needed * PAGE_SIZE,
            "not all KVA pages used (used %d, expected %d)",
            (kva_avail_ptr - kva_pages) / PAGE_SIZE, kva_pages);

        /* Now map the kernel itself */
        addr_t kernel_addr = (addr_t)&__entry & ~(PAGE_SIZE - 1);
        addr_t kernel_text_end = (addr_t)&__rodata_end & ~(PAGE_SIZE - 1);

        addr_t kernel_avail_ptr = (addr_t)kernel_pages;
        map_kernel_pages(
            kernel_pagedir,
            kernel_addr & 0x00ffffff, kernel_addr, kernel_size_in_pages, kernel_avail_ptr,
            [&](addr_t phys, addr_t virt) {
                uint64_t flags = PE_G | PE_P;
                if (virt >= kernel_text_end)
                    flags |= PE_RW;
                return flags;
            });
        KASSERT(
            kernel_avail_ptr <= (addr_t)kernel_pages + kernel_pages_needed * PAGE_SIZE,
            "not all kernel pages used (used %d, expected %d)",
            (kernel_avail_ptr - kernel_pages) / PAGE_SIZE, kernel_pages_needed);

        /* And map the dynamic KVA pages */
        addr_t dyn_kva_avail_ptr = dyn_kva_pages;
        map_kernel_pages(
            kernel_pagedir,
            0, KMEM_DYNAMIC_VA_START, dyn_kva_size_in_pages, dyn_kva_avail_ptr,
            [](addr_t phys, addr_t virt) {
                // We just setup space for mappings; not the actual mappings themselves
                return 0;
            });
        KASSERT(
            dyn_kva_avail_ptr == (addr_t)dyn_kva_pages + dyn_kva_pages_needed * PAGE_SIZE,
            "not all dynamic KVA pages used (used %d, expected %d)",
            (dyn_kva_avail_ptr - dyn_kva_pages) / PAGE_SIZE, dyn_kva_pages_needed);

        /* Activate our new page tables */
        kprintf(">>> activating kernel_pagedir = %p\n", kernel_pagedir);
        __asm("movq %0, %%cr3\n" : : "r"(kernel_pagedir));

        /*
         * Now we must update the kernel pagedir pointer to the new location, as that
         * we have paging in place.
         */
        kernel_pagedir = (uint64_t*)PTOKV((addr_t)kernel_pagedir);
        kprintf(">>> kernel_pagedir = %p\n", kernel_pagedir);
        kprintf(">>> *kernel_pagedir = %p\n", *kernel_pagedir);
        return kernel_pagedir;
    }

    void setup_memory(const struct MULTIBOOT& multiboot, addr_t& avail)
    {
        const addr_t kernel_phys_start = ((addr_t)&__entry - KERNBASE) & ~(PAGE_SIZE - 1);
        const addr_t kernel_phys_end = (avail | (PAGE_SIZE - 1)) + 1;
        kprintf(
            "setup_memory(): kernel physical memory: %p .. %p\n", kernel_phys_start,
            kernel_phys_end);
        avail = kernel_phys_end;

        // Convert multiboot memory map to chunks, excluding the kernel space
        constexpr size_t max_chunks = 32;
        struct PHYSMEM_CHUNK {
            addr_t addr, len;
        } phys_chunk[max_chunks];
        int phys_chunk_index = 0;

        addr_t mem_end = 0;
        addr_t mem_size = 0;
        const auto mm_end =
            reinterpret_cast<const char*>(multiboot.mb_mmap_addr + multiboot.mb_mmap_length);
        for (auto mm_ptr = reinterpret_cast<const char*>(multiboot.mb_mmap_addr);
             mm_ptr < mm_end && phys_chunk_index < max_chunks;) {
            const auto mm = reinterpret_cast<const MULTIBOOT_MMAP*>(mm_ptr);
            mm_ptr += mm->mm_entry_len + sizeof(uint32_t); // entry_len doesn't include size field
            if (mm->mm_type != MULTIBOOT_MMAP_AVAIL)
                continue;

            // This piece of memory is available; add it
            auto base = static_cast<uint64_t>(mm->mm_base_hi) << 32 | mm->mm_base_lo;
            auto length = static_cast<uint64_t>(mm->mm_len_hi) << 32 | mm->mm_len_lo;

            // Assume the kernel always starts at the memory chunk; if it does,
            // shift it
            if (base == kernel_phys_start) {
                length = (base + length) - kernel_phys_end;
                base = kernel_phys_end;
                kprintf(
                    "reserved kernel memory: %p .. %p (%d KB)\n", kernel_phys_start,
                    kernel_phys_end, (kernel_phys_end - kernel_phys_start) / 1024);
            }

            kprintf(
                "physical memory chunk: %p .. %p (%d KB)\n", base, base + length - 1,
                (length / 1024));
            if (phys_chunk_index == max_chunks)
                continue;
            phys_chunk[phys_chunk_index] = {base, length};
            ++phys_chunk_index;
            if (mem_end < base + length)
                mem_end = base + length;
            mem_size += length;
        }
        kprintf("setup_paging(): total memory available: %d KB\n", mem_size / 1024);

        uint64_t prev_avail = avail;
        new (kernel_vmspace) VMSpace;
        kernel_vmspace->vs_md_pagedir = setup_paging(avail, mem_end, kernel_phys_end - kernel_phys_start);

        // All memory is accessible; register with the zone allocator
        for (int n = 0; n < phys_chunk_index; n++) {
            auto& p = phys_chunk[n];
            if (p.addr == prev_avail) {
                // The zone shrank as we needed some memory from it for the
                // pages: adjust here
                p.addr = avail;
                p.len -= avail - prev_avail;
                kprintf(
                    "setup_memory(): reserved %d KB of memory due to setup_paging()\n",
                    (avail - prev_avail) / 1024);
            }
            page_zone_add(p.addr, p.len);

            /*
             * In the SMP case, ensure we'll prepare allocating memory for the SMP structures
             * right after we have memory to do so - we can't bootstrap from memory >1MB
             * and this is a handy, though crude way to avoid it.
             */
            if (n == 0)
                smp::Prepare();
        }
    }

    void InitializeTSS(TSS& tss)
    {
        memset(&tss, 0, sizeof(struct TSS));
        tss.ist1 = (addr_t)&bootstrap_stack; // emergency stack for double fault handler
    }

} // unnamed namespace

static void setup_descriptors()
{
    /*
     * Initialize a new Global Descriptor Table; we shouldn't trust what the
     * loader gives us and we'll need things like a TSS.
     */
    memset(bsp_gdt, 0, sizeof(bsp_gdt));
    SetGDTEntry(bsp_gdt, GDT_SEL_KERNEL_CODE, SEG_DPL_SUPERVISOR, GDTEntryType::Code);
    SetGDTEntry(bsp_gdt, GDT_SEL_KERNEL_DATA, SEG_DPL_SUPERVISOR, GDTEntryType::Data);
    SetGDTEntry(bsp_gdt, GDT_SEL_USER_CODE, SEG_DPL_USER, GDTEntryType::Code);
    SetGDTEntry(bsp_gdt, GDT_SEL_USER_DATA, SEG_DPL_USER, GDTEntryType::Data);
    SetGDTEntryTSS(bsp_gdt, GDT_SEL_TASK, 0, (addr_t)&bsp_tss, sizeof(struct TSS));

    /*
     * Construct the IDT; this ensures we can handle exceptions, which is useful
     * and we'll want to have as soon as possible.
     */
    memset(idt, 0, sizeof(idt));
    SetIDTEntry(idt, 0, SEG_TGATE_TYPE, 0, &exception0);
    SetIDTEntry(idt, 1, SEG_TGATE_TYPE, 0, &exception1);
    SetIDTEntry(idt, 2, SEG_TGATE_TYPE, 0, &exception2);
    SetIDTEntry(idt, 3, SEG_TGATE_TYPE, 0, &exception3);
    SetIDTEntry(idt, 4, SEG_TGATE_TYPE, 0, &exception4);
    SetIDTEntry(idt, 5, SEG_TGATE_TYPE, 0, &exception5);
    SetIDTEntry(idt, 6, SEG_TGATE_TYPE, 0, &exception6);
    SetIDTEntry(idt, 7, SEG_TGATE_TYPE, 0, &exception7);
    SetIDTEntry(idt, 8, SEG_TGATE_TYPE, 1, &exception8); /* use IST1 for double fault */
    SetIDTEntry(idt, 9, SEG_TGATE_TYPE, 0, &exception9);
    SetIDTEntry(idt, 10, SEG_TGATE_TYPE, 0, &exception10);
    SetIDTEntry(idt, 11, SEG_TGATE_TYPE, 0, &exception11);
    SetIDTEntry(idt, 12, SEG_TGATE_TYPE, 0, &exception12);
    SetIDTEntry(idt, 13, SEG_TGATE_TYPE, 0, &exception13);
    /*
     * Page fault must disable interrupts to ensure %cr2 (fault address) will not
     * be overwritten; it will re-enable the interrupt flag when it's safe to do
     * so.
     */
    SetIDTEntry(idt, 14, SEG_IGATE_TYPE, 0, &exception14);
    SetIDTEntry(idt, 16, SEG_TGATE_TYPE, 0, &exception16);
    SetIDTEntry(idt, 17, SEG_TGATE_TYPE, 0, &exception17);
    SetIDTEntry(idt, 18, SEG_TGATE_TYPE, 0, &exception18);
    SetIDTEntry(idt, 19, SEG_TGATE_TYPE, 0, &exception19);
    // Use interrupt gates for IRQ's so that we can keep track of the nesting
    // XXX We only map IRQ0..31 here; we should map everything or do things on-demand...
    for (int n = 0; n < 32; n++) {
        SetIDTEntry(idt, (32 + n), SEG_IGATE_TYPE, 0, &lapic_irq_range_1);
    }

    SetIDTEntry(idt, SMP_IPI_PERIODIC, SEG_TGATE_TYPE, SEG_DPL_SUPERVISOR, &ipi_periodic);
    SetIDTEntry(idt, SMP_IPI_PANIC, SEG_TGATE_TYPE, SEG_DPL_SUPERVISOR, &ipi_panic);
    SetIDTEntry(idt, 0xff, SEG_TGATE_TYPE, SEG_DPL_SUPERVISOR, &irq_spurious);
}

static void setup_cpu(addr_t gdt, addr_t pcpu, TSS& tss)
{
    InitializeTSS(tss);

    const auto idtr = MakeRegister(idt, IDT_NUM_ENTRIES * 16);
    const auto gdtr = MakeRegister(gdt, GDT_LENGTH);
    __asm __volatile("" : : : "memory");

    /* Load IDT, GDT and re-load all segment register */
    __asm __volatile("lidt (%%rax)\n" : : "a"(&idtr));
    __asm __volatile("lgdt (%%rax)\n"
          "mov %%cx, %%ds\n"
          "mov %%cx, %%es\n"
          "mov %%cx, %%fs\n"
          "mov %%cx, %%gs\n"
          "mov %%cx, %%ss\n"
          /* Jump to our new %cs */
          "pushq	%%rbx\n"
          "pushq	$1f\n"
          "lretq\n"
          "1:\n"
          /* Load our task register */
          "ltr	%%dx\n"
          :
          : "a"(&gdtr), "b"(GDT_SEL_KERNEL_CODE), "c"(GDT_SEL_KERNEL_DATA), "d"(GDT_SEL_TASK));

    /* Set up the userland %fs/%gs base adress to zero (it should be, but don't take chances) */
    wrmsr(MSR_FS_BASE, 0);
    wrmsr(MSR_GS_BASE, 0);

    /*
     * Set up the kernel / current %gs base; this points to our per-cpu data; the
     * current %gs base must be set because the interrupt code will not swap it
     * if we came from kernel context.
     */
    wrmsr(MSR_GS_BASE, pcpu); /* XXX why do we need this? */
    wrmsr(MSR_KERNEL_GS_BASE, pcpu);

    /*
     * Set up the fast system call (SYSCALL/SYSEXIT) mechanism; this is our way
     * of doing system calls.
     */
    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | MSR_EFER_SCE);
    wrmsr(
        MSR_STAR, ((uint64_t)(GDT_SEL_USER_CODE - 0x10) | SEG_DPL_USER) << 48L |
                      ((uint64_t)GDT_SEL_KERNEL_CODE << 32L));
    wrmsr(MSR_LSTAR, (addr_t)&syscall_handler);
    wrmsr(MSR_SFMASK, 0x200 /* IF */);

    /* Enable global pages */
    write_cr4(read_cr4() | 0x80); /* PGE */

    /* Enable FPU use; the kernel will save/restore it as needed */
    write_cr4(read_cr4() | 0x600); /* OSFXSR | OSXMMEXCPT */

    // Enable No-Execute Enable bit
    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | MSR_EFER_NXE);

    // Enable the write-protect bit; this ensures kernel-code can't write to readonly pages
    write_cr0(read_cr0() | CR0_WP);
}

static void setup_multiboot(const struct MULTIBOOT& multiboot, addr_t& avail, char*& boot_args)
{
    avail = reinterpret_cast<addr_t>(&__end) - KERNBASE;
    if (multiboot.mb_flags & MULTIBOOT_FLAG_CMDLINE) {
        // Copy boot arguments
        const auto mb_cmdline = reinterpret_cast<const char*>(multiboot.mb_cmdline);
        const auto mb_cmdline_len = strlen(mb_cmdline) + 1 /* \0 */;
        boot_args = reinterpret_cast<char*>(KERNBASE | avail);
        avail += mb_cmdline_len;
        memcpy(boot_args, mb_cmdline, mb_cmdline_len);
    }

    if (multiboot.mb_flags & MULTIBOOT_FLAG_VBE) {
        const auto mode_info = reinterpret_cast<uint8_t*>(multiboot.mb_vbe_mode_info);
        const auto bpp = *reinterpret_cast<uint8_t*>(mode_info + 0x19);
        if (bpp == 32) {
            video_resolution_width = *reinterpret_cast<uint16_t*>(mode_info + 0x12);
            video_resolution_height = *reinterpret_cast<uint16_t*>(mode_info + 0x14);
            video_framebuffer = *reinterpret_cast<uint32_t*>(mode_info + 0x28);
        }
    }
}

extern "C" void smp_ap_startup(struct X86_CPU* cpu)
{
    setup_cpu((addr_t)cpu->cpu_gdt, (addr_t)cpu->cpu_pcpu, *reinterpret_cast<TSS*>(cpu->cpu_tss));
}

static void usupport_init()
{
    usupport = page_alloc_length_mapped(PAGE_SIZE, usupport_page, VM_FLAG_READ | VM_FLAG_WRITE);
    memset(usupport, 0xf4 /* hlt */, PAGE_SIZE);

    extern char usupport_start, usupport_end;
    KASSERT(&usupport_end - &usupport_start < PAGE_SIZE, "usupport too large");
    memcpy(usupport, &usupport_start, &usupport_end - &usupport_start);
}

extern "C" void md_startup(const struct MULTIBOOT* multiboot)
{
    // Create a sane GDT/IDT so we can handle extensions and such soon
    setup_descriptors();

    // Ask the PIC to mask everything; we'll initialize when we are ready
    x86_pic_mask_all();

    // Find out how quick the CPU is; this allows us to use delay()
    x86_pit_calc_cpuspeed_mhz();

    // Wire up the CPU; this ensures exceptions can be handled properly
    setup_cpu((addr_t)&bsp_gdt, (addr_t)&bsp_pcpu, bsp_tss);

    // Run the constructor list; we can at least handle exceptions and such here,
    // but there's no new/delete yet so do not allocate things from constructors!
    __run_global_ctors();

    /*
     * Process the boot information passed by the multiboot stub; this ensures
     * it cannot be overwritten and tells us where available memory for our
     * pagetables is.
     */
    addr_t avail;
    char* boot_args;
    setup_multiboot(*multiboot, avail, boot_args);

    // Initialize our memory mappings
    setup_memory(*multiboot, avail);

    // Initialize the handles; this is needed by the per-CPU code as it initialize threads XXX Is
    // this still true?
    fd::Initialize();

    // Initialize the commandline arguments, if we have any
    cmdline_init(boot_args);

    // Create kernel process; must be done before we can create threads
    process::Initialize();

    /*
     * Initialize the per-CPU thread; this needs a working memory allocator, so that is why
     * we delay it.
     */
    memset(&bsp_pcpu, 0, sizeof(bsp_pcpu));
    pcpu_init(&bsp_pcpu);
    bsp_pcpu.tss = (addr_t)&bsp_tss;

    // Switch stack to idle thread; we'll re-use the bootstrap stack for double faults
    __asm __volatile("movq %0, %%rsp\n" : : "r"(bsp_pcpu.idlethread->md_rsp));
    PCPU_SET(curthread, bsp_pcpu.idlethread);
    scheduler::ResumeThread(*bsp_pcpu.idlethread);

    // Do ACPI pre-initialization; this prepares for parsing SMP tables
    acpi_init();

    /*
     * Initialize the SMP parts - as x86 SMP relies on an APIC, we do this here
     * to prevent problems due to devices registering interrupts.
     */
    smp::Init(bsp_pcpu);

    // Prepare the userland support page
    usupport_init();

    /*
     * Enable interrupts. We do this right before the machine-independant code
     * because it will allow us to rely on interrupts when probing devices etc.
     */
    md::interrupts::Enable();

    smp::InitTimer();

    // All done - it's up to the machine-independant code now. Force a jump to
    // prevent the stack frame from being adjusted
    __asm __volatile("jmp mi_startup\n");
    /* NOTREACHED */
}
