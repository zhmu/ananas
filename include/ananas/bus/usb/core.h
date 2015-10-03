#ifndef __ANANAS_USB_CORE_H__
#define __ANANAS_USB_CORE_H__

#include <ananas/device.h>
#include <ananas/dqueue.h>
#include <ananas/lock.h>
#include <ananas/bus/usb/descriptor.h>
#include <ananas/bus/usb/pipe.h>

/* XXX This is ugly */
#define TO_REG32(x) (x)

#define USB_MAX_ENDPOINTS	16
#define USB_MAX_INTERFACES	8
#define USB_MAX_DATALEN		1024

struct USB_DEVICE;

/*
 * A standard USB endpoint.
 */
struct USB_ENDPOINT {
	int			ep_type;
	int			ep_address;
	int			ep_dir;
#define EP_DIR_IN		0
#define EP_DIR_OUT		1
	int			ep_maxpacketsize;
	int			ep_interval;
};

/*
 * USB interface.
 */
struct USB_INTERFACE {
	int			if_class;
	int			if_subclass;
	int			if_protocol;
	struct USB_ENDPOINT	if_endpoint[USB_MAX_ENDPOINTS];	/* Endpoint configuration */
	int			if_num_endpoints;		/* Number of configured endpoints */
};

/*
 * USB status; these correspond with the USB spec.
 */
enum usb_devstat_t {
	STATUS_DEFAULT,
	STATUS_ADDRESS,
	STATUS_CONFIGURED,
	STATUS_SUSPENDED
};

typedef void (*usb_xfer_callback_t)(struct USB_TRANSFER*);

/*
 * A generic transfer to an USB device; used to issue any transfer type to any
 * breakpoint.
 */
struct USB_TRANSFER {
	struct USB_DEVICE*		xfer_device;
	int				xfer_type;
#define TRANSFER_TYPE_CONTROL		1
#define TRANSFER_TYPE_INTERRUPT		2
#define TRANSFER_TYPE_BULK		3
#define TRANSFER_TYPE_ISOCHRONOUS	4
#define TRANSFER_TYPE_HUB_ATTACH_DONE	100
	int				xfer_flags;
#define TRANSFER_FLAG_READ		0x0001
#define TRANSFER_FLAG_WRITE		0x0002
#define TRANSFER_FLAG_DATA		0x0004
#define TRANSFER_FLAG_ERROR		0x0008
#define TRANSFER_FLAG_PENDING		0x0010
	int				xfer_address;
	int				xfer_endpoint;
	/* XXX This may be a bit too much */
	struct USB_CONTROL_REQUEST	xfer_control_req;
	uint8_t				xfer_data[USB_MAX_DATALEN];
	int				xfer_length;
	int				xfer_result_length;
	usb_xfer_callback_t		xfer_callback;
	void*				xfer_callback_data;
	semaphore_t			xfer_semaphore;

	/* List of pending transfers */
	DQUEUE_FIELDS_IT(struct USB_TRANSFER, pending);

	/* List of completed transfers */
	DQUEUE_FIELDS_IT(struct USB_TRANSFER, completed);
};

DQUEUE_DEFINE(USB_TRANSFER_QUEUE, struct USB_TRANSFER);

#endif /* __ANANAS_USB_CORE_H__ */
