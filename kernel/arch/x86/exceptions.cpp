static const char* x86_exceptions[] = {
	"Divide Error", "Debug Exception", "NMI", "Breakpoint Exception", 
	"Overflow Exception", "BOUND Range Exceeded Exception",
	"Invalid Opcode", "Device Not Available", "Double Fault",
	"Coprocessor Segment Overrun", "Invalid TSS Exception",
	"Segment Not Present", "Stack Fault Exception",
	"General Protection Fault", "Page Fault Exception",
	"15", "x87 FPU Floating-Point Error",
	"Alignment Check Exception", "Machine Check Exception",
	"SIMD Floating Point Exception"
};

const char*
x86_exception_name(int num)
{
	if (num < 0 || num >= sizeof(x86_exceptions) / sizeof(char*))
		return "?";
	return x86_exceptions[num];
}
