#ifndef __ANANAS_USB_TRANSFER_H__
#define __ANANAS_USB_TRANSFER_H__

#include <ananas/types.h>

namespace Ananas {
namespace USB {

class USBDevice;

/*
 * A generic transfer to an USB device; used to issue any transfer type to any
 * breakpoint.
 *
 * This structure is locked using the device lock (xfer_device); all members
 * that are protected using that lock are marked with a [D].
 *
 * Static members are marked [S] and are not locked.
 */
class Transfer
{
public:
	Transfer(USBDevice& dev, int type, int flags)
	 : t_device(dev), t_type(type), t_flags(flags)
	{
	}

	Transfer(const Transfer&) = delete;
	Transfer& operator=(const Transfer&) = delete;

	USBDevice&			t_device;	/* [S] */
	int				t_type;		/* [S] */
#define TRANSFER_TYPE_CONTROL		1
#define TRANSFER_TYPE_INTERRUPT		2
#define TRANSFER_TYPE_BULK		3
#define TRANSFER_TYPE_ISOCHRONOUS	4
#define TRANSFER_TYPE_HUB_ATTACH_DONE	100
	int				t_flags;	/* [D] */
#define TRANSFER_FLAG_READ		0x0001
#define TRANSFER_FLAG_WRITE		0x0002
#define TRANSFER_FLAG_DATA		0x0004
#define TRANSFER_FLAG_ERROR		0x0008
#define TRANSFER_FLAG_PENDING		0x0010
	int				t_data_toggle;
	int				t_address;	/* [S] */
	int				t_endpoint;	/* [S] */
	/* XXX This may be a bit too much */
	struct USB_CONTROL_REQUEST	t_control_req;
	uint8_t				t_data[USB_MAX_DATALEN];
	int				t_length;
	int				t_result_length;
	usb_xfer_callback_t		t_callback = nullptr;
	void*				t_callback_data;
	semaphore_t			t_semaphore;
	/* HCD-specific */
	void*				t_hcd = nullptr;	/* [D] */

	/* List of pending transfers */
	LIST_FIELDS_IT(Transfer, pending);

	/* List of completed transfers */
	LIST_FIELDS_IT(Transfer, completed);
};

Transfer* AllocateTransfer(USBDevice& dev, int type, int flags, int endpt, size_t maxlen);

errorcode_t ScheduleTransfer(Transfer& xfer);
void FreeTransfer(Transfer& xfer);
void FreeTransfer_Locked(Transfer& xfer);
void CompleteTransfer(Transfer& xfer);
void CompleteTransfer_Locked(Transfer& xfer);

} // namespace USB
} // namespace Ananas

#endif /* __ANANAS_USB_TRANSFER_H__ */
