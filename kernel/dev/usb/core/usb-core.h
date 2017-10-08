#ifndef __ANANAS_USB_CORE_H__
#define __ANANAS_USB_CORE_H__

#include "kernel/list.h"
#include "descriptor.h"

/* XXX This is ugly */
#define TO_REG32(x) (x)

#define USB_MAX_ENDPOINTS	16
#define USB_MAX_INTERFACES	8
#define USB_MAX_DATALEN		1024

namespace Ananas {
namespace USB {

/*
 * A standard USB endpoint.
 */
class Endpoint
{
public:
	int		ep_type;
	int		ep_address;
	int		ep_dir;
#define EP_DIR_IN	0
#define EP_DIR_OUT	1
	int		ep_maxpacketsize;
	int		ep_interval;
};

/*
 * USB interface.
 */
class Interface
{
public:
	int		if_class;
	int		if_subclass;
	int		if_protocol;
	Endpoint	if_endpoint[USB_MAX_ENDPOINTS];	/* Endpoint configuration */
	int		if_num_endpoints;		/* Number of configured endpoints */
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

class Transfer;

typedef void (*usb_xfer_callback_t)(Transfer&);

LIST_DEFINE(TransferQueue, Transfer);

} // namespace USB
} // namespace Ananas

#endif /* __ANANAS_USB_CORE_H__ */
