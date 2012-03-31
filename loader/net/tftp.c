#include <loader/lib.h>
#include <loader/tftp.h>
#include <loader/udp.h>
#include <loader/platform.h>
#include <loader/vfs.h>

#ifdef TFTP

#define TFTP_PORT 69
#define TFTP_CLIENT_PORT 2069
#define TFTP_BLOCK_SIZE 512
#define TFTP_PACKET_SIZE (TFTP_BLOCK_SIZE + 64)

static uint8_t tftp_packet[TFTP_PACKET_SIZE];

/*
 * TFTP only allows to read TFTP_BLOCK_SIZE chunks at a time, so we use this
 * value to determine how much we have left in our input buffer.
 */
static uint32_t tftp_readbuf_offset;
static uint32_t tftp_readbuf_left;

static size_t
tftp_read_next_block()
{
	/* Wait for the next block */
	size_t s = udp_read(tftp_packet, sizeof(tftp_packet));
	if (s == 0)
		return 0; /* timeout */

	/*
	 * The reply must be DATA; if it's anything else, error out - note that
	 * both DATA and ERROR are at least 4 bytes.
	 */
	if (s < 4 || tftp_packet[0] != 0)
		return 0;
 	if (tftp_packet[1] != TFTP_OPCODE_DATA)
		return 0;

	/*
	 * ACK the reply - the data to do this is on the stack because it's only a little and
	 * we can keep tftp_packet[] filled with TFTP data this way
	 */
	uint8_t ack_packet[4];
	int n = 0;
	ack_packet[n++] = 0;
	ack_packet[n++] = TFTP_OPCODE_ACK;
	ack_packet[n++] = tftp_packet[2]; /* block number hi */
	ack_packet[n++] = tftp_packet[3]; /* block number lo */
	if (!udp_write(ack_packet, n))
		return 0;

	/* all went ok; now skip the first 4 bytes (opcode + block#) */
	s -= 4;
	tftp_readbuf_offset = 4;

	/* And fill out the remaining lengths */
	tftp_readbuf_left = s;
	vfs_curfile_offset += s;
	return 1;
}

static int
tftp_open(const char* name)
{
	extern uint32_t pxe_server_ip; /* XXX */
	if (!udp_open(pxe_server_ip, TFTP_PORT))
		return 0;

	/* Create and transmit the read request */
	int n = 0;
	tftp_packet[n++] = 0;
	tftp_packet[n++] = TFTP_OPCODE_RRQ;
	strcpy(&tftp_packet[n], name); n += strlen(name) + 1 /* terminating \0 */ ;
	strcpy(&tftp_packet[n], "octet"); n += 5 + 1 /* terminating \0 */ ;
	if (!udp_write(tftp_packet, n))
		return 0;

	/* Fetch the first block; this will also tell us whether the request worked */
	vfs_curfile_offset = 0;
	return tftp_read_next_block();
}

static void
tftp_close()
{
	udp_close();
}

static size_t
tftp_read(void* ptr, size_t length)
{
	size_t numread = 0;
	while (length > 0) {
		/* If we have some left in our read buffer, use it */
		if (tftp_readbuf_left > 0) {
			int chunklen = (tftp_readbuf_left < length) ? tftp_readbuf_left : length;
			if (ptr != NULL)
				memcpy((ptr + numread), (tftp_packet + tftp_readbuf_offset), chunklen);
			numread += chunklen; length -= chunklen;
			tftp_readbuf_offset += chunklen;
			tftp_readbuf_left -= chunklen;
			continue;
		}

		/* Nothing left; prepare for reading more */
		if (!tftp_read_next_block())
			break;
	}
	return numread;
}

static int
tftp_mount(int iodevice)
{
	/* Always succeeds if asked to mount the network disk */
	return (iodevice == -1);
}

static int
tftp_seek(uint32_t offset)
{
	/*
	 * TFTP is streaming, so we can only seek forwards. Note that we can only
	 * read in chunks of TFTP_PACKET_SIZE, and vfs_curfile_offset thus is
	 * always a multiple of this.
	 */
	unsigned int real_offset = vfs_curfile_offset - tftp_readbuf_left;
	if (real_offset >= offset)
		return real_offset == offset;

	/*
	 * Now, we just have to throw away as much as needed.
	 */
	return tftp_read(NULL, offset - real_offset) == (offset - real_offset);
}

struct LOADER_FS_DRIVER loaderfs_tftp = {
	.mount = tftp_mount,
	.open  = tftp_open,
	.close = tftp_close,
	.read  = tftp_read,
	.seek  = tftp_seek
};

#endif

/* vim:set ts=2 sw=2: */
