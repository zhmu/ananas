TARGET:		kernel.i386

OBJS=		kern/startup.o lib/memset.o lib/memcpy.o
CPPFLAGS=	-m32 -march=i386 -I../include -I.
ASFLAGS=	-D__ASSEMBLY
I386_OBJS=	arch/i386/stub.o arch/i386/startup.o arch/i386/interrupts.o \
		arch/i386/exception.o arch/i386/realmode.o \
		arch/i386/realcode.o arch/i386/vm.o

kernel.i386:	$(OBJS) conf/ld.i386 $(I386_OBJS)
		$(LD) -b elf32-i386 -T conf/ld.i386 -o kernel.i386 $(I386_OBJS) $(OBJS)

clean:
		rm -f $(OBJS) $(I386_OBJS)
