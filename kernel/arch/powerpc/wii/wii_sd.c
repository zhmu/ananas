#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/bio.h>
#include <ananas/wii/ios.h>
#include <ananas/wii/sd.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/trace.h>
#include <ananas/mm.h>
#include <mbr.h> /* XXX */

TRACE_SETUP;

struct WIISD_PRIVDATA {
	int fd;							/* IOS file handle */
	uint32_t rca;				/* Card Address Register */
	uint32_t status;
};

#define SD_SECTOR_SIZE 512

static int
wiisd_hc_read(device_t dev, int reg, uint32_t* val)
{
	struct WIISD_PRIVDATA* priv = dev->privdata;
	uint32_t op[6];

	memset(op, 0, sizeof(op));
	op[0] = reg;
	op[3] = 1;
	return ios_ioctl(priv->fd, SD_HC_READ8, op, sizeof(op), val, sizeof(*val));
}

static int
wiisd_hc_write(device_t dev, int reg, uint32_t val)
{
	struct WIISD_PRIVDATA* priv = dev->privdata;
	uint32_t op[6];

	memset(op, 0, sizeof(op));
	op[0] = reg;
	op[3] = 1;
	op[4] = val;
	return ios_ioctl(priv->fd, SD_HC_WRITE8, op, sizeof(op), NULL, 0);
}

static int
wiisd_command(device_t dev, uint32_t cmd, uint32_t cmd_type, uint32_t resp_type, uint32_t arg, uint32_t count, uint32_t sector_size, const void* buffer, void* reply)
{
	struct WIISD_PRIVDATA* priv = dev->privdata;
	uint32_t op[9];

	/* Construct the operation command block */
	op[0] = cmd;
	op[1] = cmd_type;
	op[2] = resp_type;
	op[3] = arg;
	op[4] = count;
	op[5] = sector_size;
	op[6] = (uint32_t)buffer;
	op[7] = 0;
	op[8] = 0;
	return ios_ioctl(priv->fd, SD_COMMAND, op, sizeof(op), reply, 16);
}

static int
wiisd_data_command(device_t dev, uint32_t cmd, uint32_t cmd_type, uint32_t resp_type, uint32_t arg, uint32_t count, uint32_t sector_size, void* buffer, void* reply)
{
	struct WIISD_PRIVDATA* priv = dev->privdata;

	struct ioctlv iovec[3];
	uint32_t op[9];

	/* Construct the operation command block */
	op[0] = cmd;
	op[1] = cmd_type;
	op[2] = resp_type;
	op[3] = arg;
	op[4] = count;
	op[5] = sector_size;
	op[6] = (uint32_t)buffer;
	op[7] = 1;
	op[8] = 0;

	/*
	 * Construct the IO vectors. Why the buffer is contained in both the operation block and
	 * the IO vector is beyond me.
	 */
	iovec[0].buffer = op;
	iovec[0].length = sizeof(op);
	iovec[1].buffer = buffer;
	iovec[1].length = sector_size * count;
	iovec[2].buffer = reply;
	iovec[2].length = 16;
	return ios_ioctlv(priv->fd, SD_COMMAND, 2, 1, iovec);
}

static int
wiisd_select(device_t dev)
{
	struct WIISD_PRIVDATA* priv = dev->privdata;
	uint32_t reply[4];

	return wiisd_command(dev, SD_CMD_SELECT, SD_TYPE_AC, SD_RESP_R1B, priv->rca, 0, 0, NULL, reply);
}

static int
wiisd_set_blocklength(device_t dev, uint32_t len)
{
	uint32_t reply[4];

	return wiisd_command(dev, SD_CMD_SET_BLOCKLEN, SD_TYPE_AC, SD_RESP_R1, len, 0, 0, NULL, reply);
}

static int
wiisd_set_clock(device_t dev, uint32_t clock)
{
	struct WIISD_PRIVDATA* priv = dev->privdata;
	return ios_ioctl(priv->fd, SD_SET_CLOCK, &clock, sizeof(clock), NULL, 0);
}

static int
wiisd_set_buswidth4(device_t dev)
{
	struct WIISD_PRIVDATA* priv = dev->privdata;
	uint32_t reply[4];

	int width = 2; /* means width 4 */
	int ret = wiisd_command(dev, SD_CMD_APP_CMD, SD_TYPE_AC, SD_RESP_R1, priv->rca, 0, 0, NULL, reply);
	if (ret)
		return ret;

	ret = wiisd_command(dev, SD_CMD_APP_SETWIDTH, SD_TYPE_AC, SD_RESP_R1, width, 0, 0, NULL, reply);
	if (ret)
		return ret;

	/* Now, we must update the host control register so that the controller knows */
	uint32_t hcr;
	ret = wiisd_hc_read(dev, SD_HD_HC, &hcr);
	if (ret)
		return ret;
	hcr |= SD_HD_HC_4BIT;
	return wiisd_hc_write(dev, SD_HD_HC, hcr);
}

static int
wiisd_read_block(device_t dev, uint32_t block, void* buffer)
{
	uint32_t reply[4];
	return wiisd_data_command(dev, SD_CMD_READ_MULTIPLE_BLOCK, SD_TYPE_AC, SD_RESP_R1, SD_SECTOR_SIZE * block, 1, SD_SECTOR_SIZE, buffer, reply);
}

static errorcode_t
wiisd_bread(device_t dev, struct BIO* bio)
{
	KASSERT(bio->length == SD_SECTOR_SIZE, "invalid length"); /* XXX */

	if (wiisd_read_block(dev, bio->io_block, bio->data)) {
		bio_set_error(bio);
		kprintf("wiisd_read(): i/o error");
		return ANANAS_ERROR(IO);
	}
	bio_set_available(bio);
	return ANANAS_ERROR_OK;
}

static errorcode_t
wiisd_attach(device_t dev)
{
	int fd = ios_open("/dev/sdio/slot0", IOS_OPEN_MODE_R);
	if (fd < 0) {
		kprintf("wiisd_attach(): unable to open sd slot\n");
		return ANANAS_ERROR(NO_DEVICE);
	}

	struct WIISD_PRIVDATA* privdata = kmalloc(sizeof(struct WIISD_PRIVDATA));
	dev->privdata = privdata;
	privdata->fd = fd;

	/* Reset the card; this will give us its address */
	uint32_t reply;
	if (ios_ioctl(fd, SD_RESET_CARD, NULL, 0, &reply, sizeof(reply))) {
		kprintf("wiisd_attach(): couldn't reset card\n");
		goto fail;
	}
	privdata->rca = reply & 0xffff0000; /* remove stuff bits */

	/* Fetch the card status; this will help us determine whether it's inserted or not */
	if (ios_ioctl(fd, SD_GET_STATUS, NULL, 0, &privdata->status, sizeof(privdata->status))) {
		kprintf("wiisd_attach(): couldn't obtain status\n");
		goto fail;
	}

	if (wiisd_select(dev)) {
		kprintf("wiisd_attach(): couldn't select device\n");
		goto fail;
	}

	if (wiisd_set_blocklength(dev, SD_SECTOR_SIZE)) {
		kprintf("wiisd_attach(): couldn't set block length\n");
		goto fail;
	}
	if (wiisd_set_buswidth4(dev)) {
		kprintf("wiisd_attach(): couldn't set bus width\n");
		goto fail;
	}
	if (wiisd_set_clock(dev, 1)) {
		kprintf("wiisd_attach(): couldn't set clock\n");
		goto fail;
	}

  /*
	 * XXX Read the first sector and pass it to the MBR code; this is crude and
	 * does not really belong here.
   */
	struct BIO* bio = bio_read(dev, 0, SD_SECTOR_SIZE);
	if (!BIO_IS_ERROR(bio)) {
		mbr_process(dev, bio);
	}
	bio_free(bio);

	return ANANAS_ERROR_OK;

fail:
	kfree(privdata);
	ios_close(fd);
	return ANANAS_ERROR(NO_DEVICE);
}

struct DRIVER drv_wiisd = {
	.name       = "sd",
	.drv_probe  = NULL,
	.drv_attach = wiisd_attach,
	.drv_bread  = wiisd_bread
};

DRIVER_PROBE(wiisd)
DRIVER_PROBE_BUS(corebus)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
