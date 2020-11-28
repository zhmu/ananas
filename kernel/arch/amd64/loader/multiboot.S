/*
 * multiboot1 compliant stub: it is to be placed before the kernel binary. This
 * code will move the kernel to the proper location and transition to long
 * mode.
 *
 * Assumptions:
 * - System is long mode-capable
 * - Kernel size is at most 15MB
 * - 3 pages from physical address 0x20000 onwards are unused
 * - Kernel entry point is hardcoded
 */
.text

.global __entry

.code32
__entry:
    jmp     entry

// boot flags: need 4kb alignment, memory info, video mode, provide offsets 
#define BOOT_FLAGS ((1 << 0) | (1 << 1) | (1 << 2) | (1 << 16))

    .align  8
multiboot_header:
    .long   0x1badb002                      // multiboot magic
    .long   BOOT_FLAGS                      // flags (4kb align, memory info, offsets)
    .long   -(0x1badb002 + BOOT_FLAGS)      // checksum
    .long   multiboot_header                // header address
    .long   __entry                         // load address
    .long   0                               // end address
    .long   0                               // bss end address
    .long   __entry                         // start address
    .long   0                               // linear graphics
    .long   1024                            // width
    .long   768                             // height
    .long   32                              // bits per pixel

    .align  16
gdt:
    .long   0               /*  0: null descriptor */
    .long   0

    .long   0               /*  8: 64-bit code */
    .long   0x00209800

    .long   0               /* 10: 64-bit data */
    .long   0x00009200

gdtr:
    .word   (gdtr - gdt)
    .long   gdt

entry:
    // Keep multiboot information structure - we expect it is below where the kernel will be
    movl    %ebx, %ebp

    // Set up stack, flags and use our GDT (we need the 64 bit code/data
    // descriptors, which whatever booted us likely won't have)
    movl    $0x7c00, %esp
    pushl   $0
    popfl

    /*
     * Copy whatever is append to us to the 1MB mark; this is where we expect our kernel to
     * live. We have no idea how much data there is, so just copy 15MB to be safe (this is
     * the maximum we can handle as this stub is at 16MB)
     */
    movl    $(15*1024*1024/4), %ecx
    movl    $__end, %esi
    movl    $0x100000, %edi
    rep     movsl

    /*
     * Set up page tables; we want to identity-map as much memory as possible.
     * Our kernel lives at 0xffffffff80100000, so as long as that is properly
     * mapped we should be good to go.
     *
     * We map everything to 0x0 .. 0x3fffffff and ignore any top bits; this
     * only requires 2MB pages which any amd64 capable CPU supports.
     */

    // Close to the entire base memory is available, so just pick some spot
    movl    $0x20000, %edi
    movl    %edi, %cr3

    // Clear out 3 pages; we never need to set the top 32 bits of any paging entry
    xorl    %eax, %eax
    movl    $(3 * 4096 / 4),%ecx
    pushl   %edi
    rep     stosl
    popl    %edi

    // Create page directory entries
    movl    $(4096 / 8), %ecx   // number of entries
    movl    $0x83, %edx         // physical addr | PS | RW | P
    movl    $0x21003, %eax      // pdpe base | RW | P
    movl    $0x22003, %ebx      // pde base | RW | P
1:  movl    %eax,    0(%edi)    // pml4 entry
    movl    %ebx, 4096(%edi)    // pdpe entry
    movl    %edx, 8192(%edi)    // pde entry
    addl    $0x200000, %edx
    addl    $8, %edi
    loop    1b

    // Go to 64 bit (long) mode
    lgdt    %cs:(gdtr)
    movl    %cr4, %eax
    orl     $(1<<5), %eax       // enable PAE
    movl    %eax, %cr4
    movl    $0xc0000080, %ecx   // EFER register
    rdmsr
    orl     $(1 << 8), %eax     // enable LME
    wrmsr
    movl    %cr0, %eax
    orl     $(1 << 31), %eax    // enable paging
    movl    %eax, %cr0
    ljmp    $0x8, $entry64

.code64

entry64:
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss

    // Transfer control to the kernel; %edi is the first argument which will contain the multiboot structure
    movl    %ebp, %edi
    movq    $0xffffffff80100000, %rbx
    jmp     *%rbx

__end:
