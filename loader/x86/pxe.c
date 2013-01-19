#include <loader/lib.h>
#include <loader/x86.h>
#include <loader/pxe.h>
#include <loader/platform.h>
#include <loader/vfs.h>
#include "param.h"

#ifdef PXE

extern void* pxe_trampoline;
extern void* pxenv_trampoline;
static struct PXE_ENVPLUS* pxeplus = NULL;
static struct PXE_BANGPXE* bangpxe = NULL;
static void* pxe_scratchpad;
/*static*/ uint32_t pxe_server_ip;
static uint32_t pxe_gateway_ip;
static uint32_t pxe_my_ip;

static uint16_t udp_source_port = 1025;

static uint32_t htonl(uint32_t n)
{
	return (n >> 24) | ((n >> 16) & 0xff) << 8 | ((n >> 8) & 0xff) << 16 | (n & 0xff) << 24;
}

static uint32_t htons(uint32_t n)
{
	return ((n >> 8) & 0xff) | (n & 0xff) << 8;
}

#define ntohs htons

static int
pxe_call(uint16_t func)
{
	struct REALMODE_REGS regs;

	x86_realmode_init(&regs);
	regs.ebx = func;
	regs.esi = (register_t)realmode_buffer;
	if (bangpxe != NULL) {
		regs.ip = (register_t)&pxe_trampoline;
		regs.ecx = bangpxe->EntryPointSP;
	} else {
		regs.ip = (register_t)&pxenv_trampoline;
		regs.ecx = pxeplus->RMEntry;
	}
	x86_realmode_call(&regs);
	return regs.eax & 0xffff;
}

int
udp_open()
{
	t_PXENV_UDP_OPEN* uo = (t_PXENV_UDP_OPEN*)realmode_buffer;
	uo->src_ip = pxe_my_ip;
	return pxe_call(PXENV_UDP_OPEN) == PXENV_EXIT_SUCCESS && uo->status == PXENV_STATUS_SUCCESS;
}

void
udp_close()
{
	pxe_call(PXENV_UDP_CLOSE);
}

int
udp_sendto(const void* buffer, size_t length, uint32_t ip, int16_t port)
{
	/* Copy our buffer; it must reside in base memory XXX length check? */
	memcpy(pxe_scratchpad, buffer, length);

	/* Write the packet */
	t_PXENV_UDP_WRITE* uw = (t_PXENV_UDP_WRITE*)realmode_buffer;
	uw->ip = ip;
	uw->gw = pxe_gateway_ip;
	uw->src_port = htons(udp_source_port);
	uw->dst_port = htons(port);
	uw->buffer_size = length;
	uw->buffer_offset = ((uint32_t)pxe_scratchpad & 0xf);
	uw->buffer_segment = ((uint32_t)pxe_scratchpad >> 4);
	return pxe_call(PXENV_UDP_WRITE) == PXENV_EXIT_SUCCESS && uw->status == PXENV_STATUS_SUCCESS;
}

size_t
udp_recvfrom(void* buffer, size_t length, uint32_t* ip, uint16_t* port)
{
	/*
	 * Note that there does not seem to be a blocking read operation, so we'll
	 * have to retry a fair number of times before giving up.
	 */
	for (int i = 0; i < 1000 /* times 1 ms = 1 second */; i++) {
		t_PXENV_UDP_READ* ur = (t_PXENV_UDP_READ*)realmode_buffer;
		memset(ur, 0, sizeof(*ur));
		ur->dst_ip = pxe_my_ip;
		ur->d_port = htons(udp_source_port);
		ur->buffer_size = length;
		ur->buffer_offset = ((uint32_t)pxe_scratchpad & 0xf);
		ur->buffer_segment = ((uint32_t)pxe_scratchpad >> 4);
		if (pxe_call(PXENV_UDP_READ) == PXENV_EXIT_SUCCESS && ur->status == PXENV_STATUS_SUCCESS) {
			/* Read worked XXX check buffer_size */
			memcpy(buffer, pxe_scratchpad, ur->buffer_size);
			if (ip != NULL)
				*ip = ur->src_ip;
			if (port != NULL)
				*port = ntohs(ur->s_port);
			return ur->buffer_size;
		}

		/* Wait for a bit before trying again */
		platform_delay(1);
	}
	return 0;
}

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
	pxeplus = (struct PXE_ENVPLUS*)((uint32_t)(regs.es << 4) + regs.ebx);

	/* We only verify the checksum; this should be enough */
	uint8_t sum = 0;
	for (unsigned int i = 0; i < pxeplus->Length; i++) {
		sum += *(uint8_t*)((void*)pxeplus + i);
	}
	if (sum != 0)
		return 0;

	printf(">> Netboot: PXE %u.%u - ",
	 pxeplus->Version >> 8, pxeplus->Version & 0xff);
	if (pxeplus->Version >= 0x201) {
		/* !PXE is available; we should prefer it */
		bangpxe = (struct PXE_BANGPXE*)((pxeplus->UNDICodeSeg << 4) + pxeplus->PXEPtr);

		/* Verify the checksum once more; should be enough once again :-) */
		sum = 0;
		for (unsigned int i = 0; i < bangpxe->StructLength; i++) {
			sum += *(uint8_t*)((void*)bangpxe + i);
		}
		if (sum != 0) {
			printf("!PXE unavailable, ");
			bangpxe = NULL;
		}
	}

	/*
	 * If we got this far, initialize some scratchpad memory; this is used to
	 * store the last TFTP packet received. It must reside in memory <1MB due to
	 * the wonderful aspects of realmode (which we are kind of forced to use,
	 * as the PXE protected mode interface is even a greater pain to use)
	 */
	pxe_scratchpad = (void*)((addr_t)realmode_buffer + 64);

	/*
	 * Note: it appears that supplying your own buffer does not work with all PXE's,
 	 * so just obtain their pointer instead.
	 */
	t_PXENV_GET_CACHED_INFO* gci = (t_PXENV_GET_CACHED_INFO*)realmode_buffer;
	memset(gci, 0, sizeof(t_PXENV_GET_CACHED_INFO));
	gci->PacketType = PXENV_PACKET_TYPE_CACHED_REPLY;
	if (pxe_call(PXENV_GET_CACHED_INFO) != PXENV_EXIT_SUCCESS) {
		printf("cannot obtain IP information - ignoring\n");
		bangpxe = NULL;
		return 0;
	}
	struct BOOTP* bootp = (struct BOOTP*)((uint32_t)(gci->BufferSegment << 4) + (uint32_t)gci->BufferOffset);
	pxe_server_ip = bootp->sip;
	pxe_gateway_ip = bootp->gip;
	pxe_my_ip = bootp->yip;
	printf("IP: %$, server IP: %$\n", pxe_my_ip, pxe_server_ip);
	return 1;
}

void
platform_cleanup_netboot()
{
	if (pxeplus == NULL)
		return; /* don't do anything if there is no PXE available at all */

	/*
	 * XXX The order here does not seem to confirm with the PXE spec, but
	 * at least on gPXE you cannot shutdown UNDI after unloading the stack;
	 * we just stop everything and get rid of the stack, that should
	 * be enough.
	 */
	pxe_call(PXENV_STOP_UNDI);
	pxe_call(PXENV_STOP_BASE);
	pxe_call(PXENV_UNLOAD_STACK);
}

#else /* !PXE */
int
platform_init_netboot()
{
	return 0;
}

void
platform_cleanup_netboot()
{
}
#endif

/* vim:set ts=2 sw=2: */
