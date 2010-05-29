#include <sys/lib.h> 
#include <loader/x86.h>
#include <loader/pxe.h>
#include <stdio.h> /* for printf */
#include "param.h"

int
platform_init_netboot()
{
	struct REALMODE_REGS regs;

	x86_realmode_init(&regs);
	regs.eax = 0x5650; /* PXE: installation check */
	regs.interrupt = 0x1a;
	x86_realmode_call(&regs);
	if ((regs.eax & 0xffff) != 0x564e)
		return 0;
	if ((regs.eflags & EFLAGS_CF) != 0)
		return 0;

	struct PXE_ENVPLUS* pxeplus = (struct PXE_ENVPLUS*)((uint32_t)(regs.es << 4) + regs.ebx);

	/* We only verify the checksum; this should be enough */
	uint8_t sum = 0;
	for (unsigned int i = 0; i < pxeplus->Length; i++) {
		sum += *(uint8_t*)((void*)pxeplus + i);
	}
	if (sum != 0)
		return 0;

	printf(">> Netboot: PXE %u.%u services available -",
	 pxeplus->Version >> 8, pxeplus->Version & 0xff);
	if (pxeplus->Version < 0x201) {
		printf("unsupported version - ignoring\n");
		return 0;
	}
	struct PXE_BANGPXE* bangpxe = (struct PXE_BANGPXE*)((pxeplus->UNDICodeSeg << 4) + pxeplus->PXEPtr);

	/* Verify the checksum once more; should be enough once again :-) */
	sum = 0;
	for (unsigned int i = 0; i < bangpxe->StructLength; i++) {
		sum += *(uint8_t*)((void*)bangpxe + i);
	}
	if (sum != 0) {
		printf("!PXE unavailable - ignoring\n");
		return 0;
	}

	printf("ok\n");

	struct PXENV_CACHED_INFO* gci = (struct PXENV_CACHED_INFO*)&rm_buffer;
	gci->PacketType = PXENV_PACKET_TYPE_DHCP_ACK;
	gci->BufferSize = 0x1000;
	gci->BufferOffset = 0;
	gci->BufferSegment = 0x50;

	x86_realmode_init(&regs);
	x86_realmode_push16(&regs, REALLOC_BASE >> 4);
	x86_realmode_push16(&regs, REALMODE_BUFFER);
	x86_realmode_push16(&regs, PXENV_GET_CACHED_INFO);
	regs.cs = bangpxe->EntryPointSP >> 16;
	regs.ip = bangpxe->EntryPointSP & 0xffff;
	x86_realmode_call(&regs);

	struct BOOTP* bootp = (struct BOOTP*)0x500;
	printf("My      IP: %$\n", bootp->yip);
	printf("Server  IP: %$\n", bootp->sip);
	printf("Gateway IP: %$\n", bootp->gip);

	return 1;
}

/* vim:set ts=2 sw=2: */
