#include <loader/lib.h>
#include <loader/x86.h>
#include <loader/pxe.h>
#include <loader/platform.h>
#include <loader/vfs.h>
#include "param.h"

#ifdef PXE

#define TFTP_PORT 69
#define TFTP_PACKET_SIZE 512

static struct PXE_BANGPXE* bangpxe = NULL;
#define PXE_SCRATCHPAD_LENGTH 1024
static void* pxe_scratchpad;
static uint32_t pxe_server_ip;
static uint32_t pxe_gateway_ip;
static uint32_t pxe_my_ip;

/*
 * PXE only allows to read PACKET_SIZE chunks at a time, so we
 * use this value to determine how much we have left in our
 * input buffer.
 */
static uint32_t pxe_readbuf_offset;
static uint32_t pxe_readbuf_left;

static uint32_t htonl(uint32_t n)
{
	return (n >> 24) | ((n >> 16) & 0xff) << 8 | ((n >> 8) & 0xff) << 16 | (n & 0xff) << 24;
}

static uint32_t htons(uint32_t n)
{
	return ((n >> 8) & 0xff) | (n & 0xff) << 8;
}

static int
pxe_call(uint16_t func)
{
	if (bangpxe == NULL)
		return PXENV_EXIT_FAILURE;

	struct REALMODE_REGS regs;
	x86_realmode_init(&regs);
	x86_realmode_push16(&regs, (uint16_t)((uint32_t)&rm_buffer >> 16));
	x86_realmode_push16(&regs, (uint16_t)((uint32_t)&rm_buffer & 0xffff));
	x86_realmode_push16(&regs, func);
	regs.cs = bangpxe->EntryPointSP >> 16;
	regs.ip = bangpxe->EntryPointSP & 0xffff;
	x86_realmode_call(&regs);
	return regs.eax & 0xffff;
}

static int
pxe_tftp_open(const char* name)
{
	t_PXENV_TFTP_OPEN* to = (t_PXENV_TFTP_OPEN*)&rm_buffer;
	to->ServerIPAddress = pxe_server_ip;
	to->GatewayIPAddress = pxe_gateway_ip;
	strcpy(to->FileName, name);
	to->TFTPPort = htons(TFTP_PORT);
	to->PacketSize = TFTP_PACKET_SIZE;
	if (pxe_call(PXENV_TFTP_OPEN) != PXENV_EXIT_SUCCESS || (to->Status == PXENV_STATUS_SUCCESS))
		return PXENV_STATUS_FAILURE;
	pxe_readbuf_offset = 0;
	pxe_readbuf_left = 0;
	vfs_curfile_offset = 0;
	return 1;
}

static void
pxe_tftp_close()
{
	t_PXENV_TFTP_CLOSE* tc = (t_PXENV_TFTP_CLOSE*)&rm_buffer;
	pxe_call(PXENV_TFTP_CLOSE);
}

static size_t
pxe_tftp_read(void* ptr, size_t length)
{
	size_t numread = 0;
	while (length > 0) {
		/* If we have some left in our read buffer, use it */
		if (pxe_readbuf_left > 0) {
			int chunklen = (pxe_readbuf_left < length) ? pxe_readbuf_left : length;
			if (ptr != NULL)
				memcpy((ptr + numread), (pxe_scratchpad + pxe_readbuf_offset), chunklen);
			numread += chunklen; length -= chunklen;
			pxe_readbuf_offset += chunklen;
			pxe_readbuf_left -= chunklen;
			continue;
		}

		/* Nothing left; prepare for reading more */
		pxe_readbuf_offset = 0;

		/* Have to read a new chunk */
		t_PXENV_TFTP_READ* tr = (t_PXENV_TFTP_READ*)&rm_buffer;
		tr->BufferOffset = ((uint32_t)pxe_scratchpad & 0xffff);
		tr->BufferSegment = ((uint32_t)pxe_scratchpad >> 16);
		if ((pxe_call(PXENV_TFTP_READ) != PXENV_EXIT_SUCCESS) || tr->Status != PXENV_STATUS_SUCCESS)
			break;

		pxe_readbuf_left = tr->BufferSize;
		vfs_curfile_offset += tr->BufferSize;
		if (pxe_readbuf_left == 0)
			break;
	}
	return numread;
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

	struct PXE_ENVPLUS* pxeplus = (struct PXE_ENVPLUS*)((uint32_t)(regs.es << 4) + regs.ebx);

	/* We only verify the checksum; this should be enough */
	uint8_t sum = 0;
	for (unsigned int i = 0; i < pxeplus->Length; i++) {
		sum += *(uint8_t*)((void*)pxeplus + i);
	}
	if (sum != 0)
		return 0;

	printf(">> Netboot: PXE %u.%u - ",
	 pxeplus->Version >> 8, pxeplus->Version & 0xff);
	if (pxeplus->Version < 0x201) {
		printf("unsupported version - ignoring\n");
		return 0;
	}
	bangpxe = (struct PXE_BANGPXE*)((pxeplus->UNDICodeSeg << 4) + pxeplus->PXEPtr);

	/* Verify the checksum once more; should be enough once again :-) */
	sum = 0;
	for (unsigned int i = 0; i < bangpxe->StructLength; i++) {
		sum += *(uint8_t*)((void*)bangpxe + i);
	}
	if (sum != 0) {
		printf("!PXE unavailable - ignoring\n");
		bangpxe = NULL;
		return 0;
	}

	/* If we got this far, initialize some scratchpad memory */
	pxe_scratchpad = platform_get_memory(PXE_SCRATCHPAD_LENGTH);

	/*
	 * Note: it appears that supplying your own buffer does not work with all PXE's,
 	 * so just obtain their pointer instead.
	 */
	t_PXENV_GET_CACHED_INFO* gci = (t_PXENV_GET_CACHED_INFO*)&rm_buffer;
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

static int
pxe_tftp_mount(int iodevice)
{
	/* Always succeeds if asked to mount the network disk */
	return (iodevice == -1);
}

static int
pxe_tftp_seek(uint32_t offset)
{
	/*
	 * TFTP is streaming, so we can only seek forwards. Note that we can only
	 * read in chunks of TFTP_PACKET_SIZE, and vfs_curfile_offset thus is
	 * always a multiple of this.
	 */
	unsigned int real_offset = vfs_curfile_offset - pxe_readbuf_left;
	if (real_offset >= offset)
		return real_offset == offset;

	/*
	 * Now, we just have to throw away as much as needed.
	 */
	return pxe_tftp_read(NULL, offset - real_offset) == (offset - real_offset);
}

struct LOADER_FS_DRIVER loaderfs_pxe_tftp = {
	.mount = pxe_tftp_mount,
	.open  = pxe_tftp_open,
	.close = pxe_tftp_close,
	.read  = pxe_tftp_read,
	.seek  = pxe_tftp_seek
};

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
