#ifndef __ANANAS_USB_CORE_H__
#define __ANANAS_USB_CORE_H__

#include <ananas/device.h>
#include <ananas/dqueue.h>
#include <ananas/bus/usb/descriptor.h>
#include <ananas/bus/usb/pipe.h>

/* XXX This is ugly */
#define TO_REG32(x) (x)

#define USB_MAX_ENDPOINTS	16
#define USB_MAX_INTERFACES	8
#define USB_MAX_DATALEN		1024

struct USB_DEVICE;
struct WAIT_QUEUE;

/*
 * A standard USB endpoint.
 */
struct USB_ENDPOINT {
	struct USB_DEVICE*	ep_device;
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

/*
 * An USB device consists of a name, an address, pipes and a driver. We
 * augment the existing device for this and consider the USB_DEVICE as the
 * private data for the device in question.
 */
struct USB_DEVICE {
	device_t	usb_device;			/* Device reference */
	device_t	usb_hub;			/* Hub reference */
	void*		usb_hcd_privdata;		/* HCD data for the given device */
	void*		usb_privdata;			/* Private data */
	int		usb_address;		   	/* Assigned USB address */
	int		usb_max_packet_sz0;		/* Maximum packet size for endpoint 0 */
#define USB_DEVICE_DEFAULT_MAX_PACKET_SZ0	8
	enum usb_devstat_t	usb_devstatus;		/* Device status */
	int		usb_attachstep;			/* Current attachment step */
	struct USB_INTERFACE usb_interface[USB_MAX_INTERFACES];
	int		usb_num_interfaces;
	int		usb_cur_interface;
	struct USB_DESCR_DEVICE usb_descr_device;
	struct USB_PIPES usb_pipes;

	/* Provide queue structure for device attachment */
	DQUEUE_FIELDS(struct USB_DEVICE);
};

DQUEUE_DEFINE(USB_DEVICE_QUEUE, struct USB_DEVICE);

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
	int				xfer_address;
	int				xfer_endpoint;
	/* XXX This may be a bit too much */
	struct USB_CONTROL_REQUEST	xfer_control_req;
	uint8_t				xfer_data[USB_MAX_DATALEN];
	int				xfer_length;
	int				xfer_result_length;
	usb_xfer_callback_t		xfer_callback;
	void*				xfer_callback_data;
	struct WAIT_QUEUE		xfer_waitqueue;
	/* Provide queue structure */
	DQUEUE_FIELDS(struct USB_TRANSFER);
};

DQUEUE_DEFINE(USB_TRANSFER_QUEUE, struct USB_TRANSFER);

void usb_init();
struct USB_DEVICE* usb_alloc_device(device_t root, device_t hub, void* hcd_privdata);
struct USB_TRANSFER* usb_alloc_transfer(struct USB_DEVICE* dev, int type, int flags, int endpt);
errorcode_t usb_schedule_transfer(struct USB_TRANSFER* xfer);
void usb_free_transfer(struct USB_TRANSFER* xfer);
void usb_completed_transfer(struct USB_TRANSFER* xfer);
int usb_get_next_address(struct USB_DEVICE* usb_dev);

void usb_attach_device(device_t parent, device_t hub, void* hcd_privdata);
void usb_attach_init();

#endif /* __ANANAS_USB_CORE_H__ */
