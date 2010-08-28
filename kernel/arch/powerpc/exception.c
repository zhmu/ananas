#include <sys/types.h>
#include <machine/interrupts.h>
#include <machine/thread.h>
#include <sys/syscall.h>
#include <sys/lib.h>

extern void generic_int();
extern void dsi_trap();
extern void* generic_int_len;
extern void* dsi_trap_len;
	
static const char* ppc_exceptions[] = {
	"Reserved", "System Reset", "Machine Check", "Data Storage",
	"Instruction Storage", "External", "Alignment", "Program",
	"Floating-Point Unavailable", "Decrementer", "Reserved",
	"Reserved", "System Call", "Trace", "Floating-Point Assist"
};

static uint32_t
syscall_handler(struct STACKFRAME* sf)
{
	struct SYSCALL_ARGS sa;
	sa.number = sf->sf_reg[0];
	sa.arg1 = sf->sf_reg[3];
	sa.arg2 = sf->sf_reg[4];
	sa.arg3 = sf->sf_reg[5];
	sa.arg4 = sf->sf_reg[6];
	sa.arg5 = sf->sf_reg[7];
	sf->sf_reg[3] = syscall(&sa);
}

uint32_t
exception_handler(struct STACKFRAME* sf)
{
	if (sf->sf_exc == INT_SC)
		return syscall_handler(sf);

	const char* exception_text = "Reserved";
	if ((sf->sf_exc & 0xff) == 0 && sf->sf_exc / 0x100 <= sizeof(ppc_exceptions) / sizeof(char*))
		exception_text = ppc_exceptions[sf->sf_exc / 0x100];

	kprintf(
		"exception 0x%x: %s at %x\n"
		"srr1=%x dar=%x dsisr=%x lr=%x\n"
		" r0=%x  r1=%x  r2=%x  r3=%x  r4=%x  r5=%x  r6=%x  r7=%x\n"
		" r8=%x  r9=%x r10=%x r11=%x r12=%x r13=%x r14=%x r15=%x\n"
		"r16=%x r17=%x r18=%x r19=%x r20=%x r21=%x r22=%x r23=%x\n"
		"r24=%x r25=%x r26=%x r27=%x r28=%x r29=%x r30=%x r31=%x\n",
		sf->sf_exc, exception_text, sf->sf_srr0,
	  sf->sf_srr1, sf->sf_dar, sf->sf_dsisr, sf->sf_lr,
		sf->sf_reg[ 0], sf->sf_reg[ 1], sf->sf_reg[ 2], sf->sf_reg[ 3],
		sf->sf_reg[ 4], sf->sf_reg[ 5], sf->sf_reg[ 6], sf->sf_reg[ 7],
		sf->sf_reg[ 8], sf->sf_reg[ 9], sf->sf_reg[10], sf->sf_reg[11],
		sf->sf_reg[12], sf->sf_reg[13], sf->sf_reg[14], sf->sf_reg[15],
		sf->sf_reg[16], sf->sf_reg[17], sf->sf_reg[18], sf->sf_reg[19],
		sf->sf_reg[20], sf->sf_reg[21], sf->sf_reg[22], sf->sf_reg[23],
		sf->sf_reg[24], sf->sf_reg[25], sf->sf_reg[26], sf->sf_reg[27],
		sf->sf_reg[28], sf->sf_reg[29], sf->sf_reg[30], sf->sf_reg[31]
	);
#if 0
	/* May be useful for debugging, but generally is just too much... */
	kprintf(
		" sr0=%x  sr1=%x  sr2=%x  sr3=%x\n"
		" sr4=%x  sr5=%x  sr6=%x  sr7=%x\n"
		" sr8=%x  sr9=%x sr10=%x sr11=%x\n"
		"sr12=%x sr13=%x sr14=%x sr15=%x\n",
		sf->sf_sr[ 0], sf->sf_sr[ 1], sf->sf_sr[ 2], sf->sf_sr[ 3],
		sf->sf_sr[ 4], sf->sf_sr[ 5], sf->sf_sr[ 6], sf->sf_sr[ 7],
		sf->sf_sr[ 8], sf->sf_sr[ 9], sf->sf_sr[10], sf->sf_sr[11],
		sf->sf_sr[12], sf->sf_sr[13], sf->sf_sr[14], sf->sf_sr[15]);
#endif

	panic("exception");
	return 0; /* NOTREACHED */
}

void
exception_init()
{
	/*
	 * On an interrupt, the CPU just calls a predefined location in
	 * memory; we need to copy our interrupt handling code to the
	 * appropriate location. The assembly code will eventually call
	 * exception_handler() which handles the actual interrupt.
	 */
	memcpy((void*)INT_MC,  (void*)&generic_int, (size_t)&generic_int_len);
	memcpy((void*)INT_DS,  (void*)&dsi_trap,    (size_t)&dsi_trap_len);
	memcpy((void*)INT_IS,  (void*)&generic_int, (size_t)&generic_int_len);
	memcpy((void*)INT_EXT, (void*)&generic_int, (size_t)&generic_int_len);
	memcpy((void*)INT_A,   (void*)&generic_int, (size_t)&generic_int_len);
	memcpy((void*)INT_P,   (void*)&generic_int, (size_t)&generic_int_len);
	memcpy((void*)INT_FPU, (void*)&generic_int, (size_t)&generic_int_len);
	memcpy((void*)INT_DEC, (void*)&generic_int, (size_t)&generic_int_len);
	memcpy((void*)INT_SC,  (void*)&generic_int, (size_t)&generic_int_len);
	memcpy((void*)INT_T,   (void*)&generic_int, (size_t)&generic_int_len);
	memcpy((void*)INT_FPA, (void*)&generic_int, (size_t)&generic_int_len);
}

/* vim:set ts=2 sw=2: */
