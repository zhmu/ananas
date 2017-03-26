#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/endian.h>
#include <ananas/lib.h>
#include <ananas/lock.h>
#include <ananas/thread.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/mm.h>
#include "usb-core.h"
#include "usb-device.h"
#include "config.h"
#include "pipe.h"
#include "transfer.h"

TRACE_SETUP;

struct USBSTORAGE_CBW {
	/* 00-03 */ uint32_t d_cbw_signature;
#define USBSTORAGE_CBW_SIGNATURE 0x43425355
	/* 04-07 */ uint32_t d_cbw_tag;
	/* 08-0b */ uint32_t d_cbw_data_transferlength;
	/* 0c */    uint8_t  d_bm_cbwflags;
#define USBSTORAGE_CBW_FLAG_DATA_OUT (0 << 7)
#define USBSTORAGE_CBW_FLAG_DATA_IN  (1 << 7)
	/* 0d */    uint8_t  d_cbw_lun;
	/* 0e */    uint8_t  d_cbw_cblength;
	/* 0f-1e */ uint8_t  d_cbw_cb[16];
} __attribute__((packed));

struct USBSTORAGE_CSW {
	/* 00-03 */ uint32_t d_csw_signature;
#define USBSTORAGE_CSW_SIGNATURE 0x53425355
	/* 04-07 */ uint32_t d_csw_tag;
	/* 08-0b */ uint32_t d_csw_data_residue;
	/* 0c */    uint8_t  d_csw_status;
#define USBSTORAGE_CSW_STATUS_GOOD 0x00
#define USBSTORAGE_CSW_STATUS_FAIL 0x01
#define USBSTORAGE_CSW_STATUS_PHASE_ERROR 0x02
} __attribute__((packed));

#define SCSI_CMD_TEST_UNIT_READY 0x00
#define SCSI_CMD_REQUEST_SENSE 0x3
#define SCSI_CMD_INQUIRY_6 0x12
#define SCSI_CMD_READ_CAPACITY_10	0x25
#define SCSI_CMD_READ_10 0x28

struct SCSI_CDB_6 {
	/* 00 */    uint8_t  c_code;
	/* 01 */    uint8_t  c_misc;
	/* 02-03 */ uint16_t c_lba;
	/* 04 */    uint8_t  c_alloc_len;
	/* 05 */    uint8_t  c_control;
} __attribute__((packed));

struct SCSI_CDB_10 {
	/* 00 */    uint8_t  c_code;
	/* 01 */    uint8_t  c_misc1;
	/* 02-05 */ uint32_t c_lba;
	/* 06 */    uint8_t  c_misc2;
	/* 07-08 */ uint16_t c_alloc_len;
	/* 09 */    uint8_t  c_control;
} __attribute__((packed));

struct SCSI_TEST_UNIT_READY_CMD {
	/* 00 */    uint8_t  c_code;
	/* 01-04 */ uint8_t  c_reserved[4];
	/* 05 */    uint8_t  c_control;
} __attribute__((packed));

struct SCSI_REQUEST_SENSE_CMD {
	/* 00 */    uint8_t  c_code;
	/* 01 */    uint8_t  c_desc;
	/* 02-03 */ uint8_t  c_reserved[2];
	/* 04 */    uint8_t  c_alloc_len;
	/* 05 */    uint8_t  c_control;
} __attribute__((packed));

struct SCSI_FIXED_SENSE_DATA {
	/* 00 */    uint8_t  sd_code;
#define SCSI_FIXED_SENSE_CODE_VALID (1 << 7)
#define SCSI_FIXED_SENSE_CODE_CODE(x) ((x) & 0x7f)
	/* 01 */    uint8_t  sd_obsolete;
	/* 02 */    uint8_t  sd_flags;
#define SCSI_FIXED_SENSE_FLAGS_FILEMARK (1 << 7)
#define SCSI_FIXED_SENSE_FLAGS_EOM (1 << 6)
#define SCSI_FIXED_SENSE_FLAGS_ILI (1 << 5)
#define SCSI_FIXED_SENSE_FLAGS_KEY(x) ((x) & 0xf)
	/* 03-06 */ uint8_t  sd_info[4];
	/* 07 */    uint8_t  sd_add_length;
	/* 08-0b */ uint8_t  sd_cmd_info[4];
	/* 0c */    uint8_t  sd_add_sense_code;
	/* 0d */    uint8_t  sd_add_sense_qualifier;
	/* 0e */    uint8_t  sd_field_replace_code;
	/* 0f-11 */ uint8_t  sd_sense_key_specific[3];
	/* 13-.. */ uint8_t  sd_add_sense_bytes[0];
} __attribute__((packed));

struct SCSI_READ_10_CMD {
	/* 00 */    uint8_t  c_code;
	/* 01 */    uint8_t  c_flags;
#define SCSI_READ_10_FLAG_DPO (1 << 4)
#define SCSI_READ_10_FLAG_FUA (1 << 3)
#define SCSI_READ_10_FLAG_FUA_NV (1 << 1)
	/* 02-05 */ uint32_t  c_lba;
	/* 06 */    uint8_t   c_group_number;
	/* 07-08 */ uint16_t  c_transfer_len;
	/* 09 */    uint8_t   c_control;
} __attribute__((packed));

struct SCSI_INQUIRY_6_CMD {
	/* 00 */    uint8_t  c_code;
	/* 01 */    uint8_t  c_evpd;
	/* 02 */    uint8_t  c_page_code;
	/* 03-04 */ uint16_t c_alloc_len;
	/* 05 */    uint8_t  c_control;
} __attribute__((packed));

struct SCSI_INQUIRY_6_REPLY {
	/* 00 */    uint8_t  r_peripheral;
	/* 01 */    uint8_t  r_flags1;
#define SCSI_INQUIRY_6_FLAG1_RMB (1 << 7)
	/* 02 */    uint8_t  r_version;
	/* 03 */    uint8_t  r_flags3;
	/* 04 */    uint8_t  r_add_length;
	/* 05 */    uint8_t  r_flags5;
	/* 06 */    uint8_t  r_flags6;
	/* 07 */    uint8_t  r_flags7;
	/* 08-0f */ uint8_t  r_vendor_id[8];
	/* 10-1f */ uint8_t  r_product_id[16];
	/* 20-23 */ uint8_t  r_revision[4];
} __attribute__((packed));

struct SCSI_READ_CAPACITY_10_CMD {
	/* 00 */    uint8_t  c_code;
	/* 01 */    uint8_t  c_reserved1;
	/* 02-05 */ uint32_t c_lba;
	/* 06 */    uint8_t  c_reserved2;
	/* 07 */    uint8_t  c_reserved3;
	/* 08 */    uint8_t  c_pmi;
	/* 09 */    uint8_t  c_control;
} __attribute__((packed));

struct SCSI_READ_CAPACITY_10_REPLY {
	/* 00-03 */ uint32_t r_lba;
	/* 04-07 */ uint32_t r_block_length;
} __attribute__((packed));

struct USBSTORAGE_PRIVDATA {
	mutex_t s_mutex;
	unsigned int s_max_lun;
	struct USB_PIPE* s_bulk_in;
	struct USB_PIPE* s_bulk_out;
	/* Output buffer */
	void* s_output_buffer;
	size_t s_output_filled;
	size_t s_output_len;
	/* Most recent CSW received */
	errorcode_t* s_result_ptr;
	struct USBSTORAGE_CSW* s_csw_ptr;
	/* Signalled when the CSW is received */
	semaphore_t s_signal_sem;
};

static errorcode_t
usbstorage_handle_request(device_t dev, int lun, int flags, const void* cb, size_t cb_len, void* result, size_t* result_len)
{
	auto p = static_cast<struct USBSTORAGE_PRIVDATA*>(dev->privdata);
	errorcode_t err = ANANAS_ERROR(UNKNOWN);
	struct USBSTORAGE_CSW csw;
	KASSERT(result_len == NULL || result != NULL, "result_len without result?");

	kprintf("p %p flags %d len %d cb_len %d result_len %d\n", p, flags, lun, cb_len, result_len ? *result_len : -1);

	/* Create the command-block-wrapper */
	struct USBSTORAGE_CBW cbw;
	memset(&cbw, 0, sizeof cbw);
	cbw.d_cbw_signature = USBSTORAGE_CBW_SIGNATURE;
	cbw.d_cbw_data_transferlength = (result_len != NULL) ? *result_len : 0;
	cbw.d_bm_cbwflags = flags;
	cbw.d_cbw_lun = lun;
	cbw.d_cbw_cblength = cb_len;
	memcpy(&cbw.d_cbw_cb[0], cb, cb_len);

	/* All SCSI CDB's follow a standard format; fill out those fields here */
	switch(cb_len) {
		case 6: {
			struct SCSI_CDB_6* cdb = (struct SCSI_CDB_6*)&cbw.d_cbw_cb[0];
			cdb->c_alloc_len = (result_len != NULL) ? *result_len : 0;
			break;
		}
		case 10: {
			struct SCSI_CDB_10* cdb = (struct SCSI_CDB_10*)&cbw.d_cbw_cb[0];
			uint16_t v = (result_len != NULL) ? *result_len : 0;
			if (cdb->c_code != SCSI_CMD_READ_10) /* XXX */
				cdb->c_alloc_len = htobe16(v);
			break;
		}
		default:
			panic("invalid cb_len %d", cb_len);
	}

	mutex_lock(&p->s_mutex);
	/* Ensure our output is at a sensible location */
	p->s_output_buffer = result;
	p->s_output_filled = 0;
	p->s_output_len = (result_len != NULL) ? *result_len : 0;
	p->s_result_ptr = &err;
	p->s_csw_ptr = &csw;
	/* Now, submit the request */
	p->s_bulk_out->p_xfer->xfer_length = sizeof(cbw);
	memcpy(&p->s_bulk_out->p_xfer->xfer_data[0], &cbw, p->s_bulk_out->p_xfer->xfer_length);

	kprintf(">>> submitting cbw\n");
	for (int n = 0; n < sizeof(cbw); n++)
		kprintf("%x ", p->s_bulk_out->p_xfer->xfer_data[n]);
	kprintf("\n");

	usbpipe_schedule(p->s_bulk_in);
	usbpipe_schedule(p->s_bulk_out);
	mutex_unlock(&p->s_mutex);

	/* Now we wait for the signal ... */
	sem_wait_and_drain(&p->s_signal_sem);
	ANANAS_ERROR_RETURN(err);

	/* See if the CSW makes sense */
	if (csw.d_csw_signature != USBSTORAGE_CSW_SIGNATURE)
		return ANANAS_ERROR(IO);
	if (csw.d_csw_tag != cbw.d_cbw_tag)
		return ANANAS_ERROR(IO);
	if (csw.d_csw_status != USBSTORAGE_CSW_STATUS_GOOD) {
		device_printf(dev, "device rejected request: %d", csw.d_csw_status);
		return ANANAS_ERROR(IO);
	}

	return ananas_success();
}

static errorcode_t
usbstorage_probe(Ananas::ResourceSet& resourceSet)
{
	auto usb_dev = static_cast<struct USB_DEVICE*>(resourceSet.AllocateResource(Ananas::Resource::RT_USB_Device, 0));
	if (usb_dev == NULL)
		return ANANAS_ERROR(NO_DEVICE);

	/* XXX This is crude */
	struct USB_INTERFACE* iface = &usb_dev->usb_interface[usb_dev->usb_cur_interface];
	if (iface->if_class != USB_IF_CLASS_STORAGE || iface->if_protocol != USB_IF_PROTOCOL_BULKONLY)
		return ANANAS_ERROR(NO_DEVICE);

	return ananas_success();
}

/* Called when data flows from the device -> us */
static void
usbstorage_in_callback(struct USB_PIPE* pipe)
{
	device_t dev = pipe->p_dev->usb_device;
	auto p = static_cast<struct USBSTORAGE_PRIVDATA*>(dev->privdata);
	struct USB_TRANSFER* xfer = pipe->p_xfer;

	kprintf("usbstorage_in_callback! -> p %p flags %x len %d\n", p, xfer->xfer_flags, xfer->xfer_result_length);

	/*
	 * We'll have one or two responses now: the first will be the resulting data,
	 * and the second will be the CSW.
	 */
	int need_schedule = 0;
	mutex_lock(&p->s_mutex);
	int len = xfer->xfer_result_length;
	if (p->s_output_buffer != NULL) {
		size_t left = p->s_output_len - p->s_output_filled;
		if (len > left)
			len = left;

		memcpy((char*)p->s_output_buffer + p->s_output_filled, &xfer->xfer_data[0], len);
		p->s_output_filled += len;
		left -= len;
		if (left == 0) {
			p->s_output_len = len;
			p->s_output_buffer = NULL;
		}
		need_schedule = 1; /* as more data will arrive */
	} else if (p->s_csw_ptr != NULL && p->s_result_ptr != NULL) {
		if (len != sizeof(struct USBSTORAGE_CSW)) {
			device_printf(dev, "invalid csw length (expected %d got %d)", sizeof(struct USBSTORAGE_CSW), len);
			*p->s_result_ptr = ANANAS_ERROR(BAD_LENGTH);
		} else {
			memcpy(p->s_csw_ptr, &xfer->xfer_data[0], len);
			*p->s_result_ptr = ananas_success();
		}
		p->s_result_ptr = NULL;
		p->s_csw_ptr = NULL;
		sem_signal(&p->s_signal_sem);
	} else {
		device_printf(dev, "received %d bytes but no sink?", len);
	}
	mutex_unlock(&p->s_mutex);

	if (need_schedule)
		usbpipe_schedule(pipe);
}

static void
usbstorage_out_callback(struct USB_PIPE* pipe)
{
	struct USB_TRANSFER* xfer = pipe->p_xfer;

	kprintf("usbstorage_out_callback! -> len %d\n", xfer->xfer_result_length);
}

static void
copy_string(char* dest, int dest_len, const uint8_t* src, int src_len)
{
	/* First step is to copy with \0-termination */
	memcpy(dest, src, src_len > dest_len ? dest_len : src_len);
	dest[dest_len] = '\0';

	/* Remove any trailing whitespace and \0-terminate */
	int n = dest_len - 1;
	while (n > 0 && (dest[n] == '\0' || dest[n] == ' '))
		n--;
	dest[n] = '\0';
}

static void
scsi_dump_sense_data(const struct SCSI_FIXED_SENSE_DATA* sd)
{
	kprintf("sense data: code %d (%c) flags %c%c%c key %d\n",
	 SCSI_FIXED_SENSE_CODE_CODE(sd->sd_code),
	 (sd->sd_code & SCSI_FIXED_SENSE_CODE_VALID) ? 'V' : '.',
	 (sd->sd_flags & SCSI_FIXED_SENSE_FLAGS_FILEMARK) ? 'F' : '.',
	 (sd->sd_flags & SCSI_FIXED_SENSE_FLAGS_EOM) ? 'E' : '.',
	 (sd->sd_flags & SCSI_FIXED_SENSE_FLAGS_ILI) ? 'I' : '.',
	 SCSI_FIXED_SENSE_FLAGS_KEY(sd->sd_flags));
	kprintf("info 0x%x 0x%x 0x%x 0x%x add length %d\n",
	 sd->sd_info[0], sd->sd_info[1],
	 sd->sd_info[2], sd->sd_info[3],
	 sd->sd_add_length);
	kprintf("command info 0x%x 0x%x 0x%x 0x%x\n",
	 sd->sd_cmd_info[0], sd->sd_cmd_info[1],
	 sd->sd_cmd_info[2], sd->sd_cmd_info[3]);
	kprintf("add sense code %d qualifier %d field replace code %d\n",
	 sd->sd_add_sense_code, sd->sd_add_sense_qualifier,
	 sd->sd_field_replace_code);
}

static errorcode_t
usbstorage_attach(device_t dev)
{
	auto usb_dev = static_cast<struct USB_DEVICE*>(dev->d_resourceset.AllocateResource(Ananas::Resource::RT_USB_Device, 0));

	auto p = new(dev) USBSTORAGE_PRIVDATA;
	memset(p, 0, sizeof *p);
	dev->privdata = p;
	usb_dev->usb_device->privdata = p;
	mutex_init(&p->s_mutex, "usbstorage");
	sem_init(&p->s_signal_sem, 0);

	/* Determine the max LUN of the device */
	uint8_t max_lun = 0xff;
	size_t len = sizeof(max_lun);
	errorcode_t err = usb_control_xfer(usb_dev, USB_CONTROL_REQUEST_GET_MAX_LUN, USB_CONTROL_RECIPIENT_INTERFACE, USB_CONTROL_TYPE_CLASS, USB_REQUEST_MAKE(0, 0), 0, &max_lun, &len, 0);
	if (ananas_is_failure(err)) {
		device_printf(dev, "unable to get max LUN: %d", err);
		return ANANAS_ERROR(NO_RESOURCE);
	}
	p->s_max_lun = max_lun;

	/*
	 * There must be a BULK/IN and BULK/OUT endpoint - however, the spec doesn't
	 * say in which order they are. To cope, we'll just try both.
	 *
	 */
	err = usbpipe_alloc(usb_dev, 0, TRANSFER_TYPE_BULK, EP_DIR_IN, 0, usbstorage_in_callback, &p->s_bulk_in);
	if (ananas_is_failure(err))
		err = usbpipe_alloc(usb_dev, 1, TRANSFER_TYPE_BULK, EP_DIR_IN, 0, usbstorage_in_callback, &p->s_bulk_in);
	if (ananas_is_failure(err)) {
		device_printf(dev, "no bulk/in endpoint present");
		return ANANAS_ERROR(NO_RESOURCE);
	}
	err = usbpipe_alloc(usb_dev, 1, TRANSFER_TYPE_BULK, EP_DIR_OUT, 0, usbstorage_out_callback, &p->s_bulk_out);
	if (ananas_is_failure(err))
		err = usbpipe_alloc(usb_dev, 0, TRANSFER_TYPE_BULK, EP_DIR_OUT, 0, usbstorage_out_callback, &p->s_bulk_out);
	if (ananas_is_failure(err)) {
		device_printf(dev, "no bulk/out endpoint present");
		return ANANAS_ERROR(NO_RESOURCE);
	}

	/* Do a SCSI INQUIRY command; we only use the vendor/product ID for now */
	struct SCSI_INQUIRY_6_REPLY inq_reply;
	struct SCSI_INQUIRY_6_CMD inq_cmd;
	memset(&inq_cmd, 0, sizeof inq_cmd);
	inq_cmd.c_code = SCSI_CMD_INQUIRY_6;

	size_t reply_len = sizeof(inq_reply);
	err = usbstorage_handle_request(dev, 0, USBSTORAGE_CBW_FLAG_DATA_IN, &inq_cmd, sizeof(inq_cmd), &inq_reply, &reply_len);
	ANANAS_ERROR_RETURN(err);

	/*
	 * We expect the device to be unavailable (as it's just reset) - we'll issue
	 * a 'TEST UNIT READY' and if that fails, we'll do a 'REQUEST SENSE' so the
	 * device ought to reset itself.
	 */
	struct SCSI_TEST_UNIT_READY_CMD tur_cmd;
	memset(&tur_cmd, 0, sizeof tur_cmd);
	tur_cmd.c_code = SCSI_CMD_TEST_UNIT_READY;
	err = usbstorage_handle_request(dev, 0, USBSTORAGE_CBW_FLAG_DATA_IN, &tur_cmd, sizeof(tur_cmd), NULL, NULL);
	if (ananas_is_failure(err)) {
		/* This is expected; issue 'REQUEST SENSE' to reset the status */
		struct SCSI_REQUEST_SENSE_CMD rs_cmd;
		struct SCSI_FIXED_SENSE_DATA sd;
		memset(&rs_cmd, 0, sizeof rs_cmd);
		rs_cmd.c_code = SCSI_CMD_REQUEST_SENSE;
		reply_len = sizeof(sd);
		err = usbstorage_handle_request(dev, 0, USBSTORAGE_CBW_FLAG_DATA_IN, &rs_cmd, sizeof(rs_cmd), &sd, &reply_len);
		kprintf("handle_req: err=%d len %d\n", err, reply_len);

		scsi_dump_sense_data(&sd);
	}

	/* Now fetch the capacity */
	struct SCSI_READ_CAPACITY_10_CMD cap_cmd;
	memset(&cap_cmd, 0, sizeof cap_cmd);
	cap_cmd.c_code = SCSI_CMD_READ_CAPACITY_10;
	struct SCSI_READ_CAPACITY_10_REPLY cap_reply;
	reply_len = sizeof(cap_reply);
	err = usbstorage_handle_request(dev, 0, USBSTORAGE_CBW_FLAG_DATA_IN, &cap_cmd, sizeof(cap_cmd), &cap_reply, &reply_len);
	ANANAS_ERROR_RETURN(err);
	uint32_t num_lba = betoh32(cap_reply.r_lba);
	uint32_t block_len = betoh32(cap_reply.r_block_length);

	/* Now print some nice information */
	char vid[9], pid[17];
	copy_string(vid, sizeof(vid), inq_reply.r_vendor_id, sizeof(inq_reply.r_vendor_id));
	copy_string(pid, sizeof(pid), inq_reply.r_product_id, sizeof(inq_reply.r_product_id));
	device_printf(dev, "vendor <%s> product <%s> size %d MB", vid, pid,
	 num_lba / ((1024 * 1024) / block_len));

	device_printf(dev, "lba %d blocklen %d", num_lba, block_len);

	/* Let's read something! */
	uint8_t block[512];
	memset(block, 0, sizeof block);

	struct SCSI_READ_10_CMD r_cmd;
	memset(&r_cmd, 0, sizeof r_cmd);
	r_cmd.c_code = SCSI_CMD_READ_10;
	r_cmd.c_lba = htobe32(0);
	r_cmd.c_transfer_len = htobe16(1);
	reply_len = sizeof(block);
	err = usbstorage_handle_request(dev, 0, USBSTORAGE_CBW_FLAG_DATA_IN, &r_cmd, sizeof(r_cmd), block, &reply_len);
	kprintf("read -> err %d len %d\n", err, reply_len);
	for (unsigned int n = 0; n < reply_len; n++) {
		kprintf("%x ", block[n]);
	}

	return ananas_success();
}

struct DRIVER drv_usbstorage = {
	.name = "usbstorage",
	.drv_probe = usbstorage_probe,
	.drv_attach = usbstorage_attach
};

DRIVER_PROBE(usbstorage)
DRIVER_PROBE_BUS(usbbus)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
