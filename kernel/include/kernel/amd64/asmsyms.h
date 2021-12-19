// PCPU struct
#define PCPU_SYSCALLRSP     0x08
#define PCPU_RSP0           0x10
#define PCPU_CURTHREAD      0x28
#define PCPU_NESTEDIRQ      0x38

// Stack frame
#define SF_TRAPNO   0x00
#define SF_RAX      0x08
#define SF_RBX      0x10
#define SF_RCX      0x18
#define SF_RDX      0x20
#define SF_RBP      0x28
#define SF_RSI      0x30
#define SF_RDI      0x38
#define SF_R8       0x40
#define SF_R9       0x48
#define SF_R10      0x50
#define SF_R11      0x58
#define SF_R12      0x60
#define SF_R13      0x68
#define SF_R14      0x70
#define SF_R15      0x78
#define SF_DS       0x80
#define SF_ES       0x82
#define SF_ERRNUM   0x88
#define SF_RIP      0x90
#define SF_CS       0x98
#define SF_RFLAGS   0xa0
#define SF_RSP      0xa8
#define SF_SS       0xb0
#define SF_SIZE     0xb8

// SMP_CPU struct
#define SMP_CPU_LAPICID     0x00
#define SMP_CPU_STACK       0x08
#define SMP_CPU_PCPU        0x10
#define SMP_CPU_SIZE        0x28

// Sysarg struct
#define SYSARG_NUM  0x00
#define SYSARG_ARG1 0x08
#define SYSARG_ARG2 0x10
#define SYSARG_ARG3 0x18
#define SYSARG_ARG4 0x20
#define SYSARG_ARG5 0x28
#define SYSARG_SIZE 0x30

// Thread members
#define T_FLAGS         0x270
#define T_SIG_PENDING   0x274
#define T_FRAME         0x280
#define T_MDFLAGS       0x288

// Vmspace struct
#define VMSPACE_MD_PAGEDIR 0x0
