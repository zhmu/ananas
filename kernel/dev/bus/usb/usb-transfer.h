#ifndef __ANANAS_USB_TRANSFER_H__
#define __ANANAS_USB_TRANSFER_H__

#include <ananas/types.h>

struct USB_DEVICE;
struct USB_TRANSFER;

errorcode_t usb_control_xfer(struct USB_DEVICE* usb_dev, int req, int recipient, int type, int value, int index, void* buf, size_t* len, int write);

void usbtransfer_init();

struct USB_TRANSFER* usbtransfer_alloc(struct USB_DEVICE* dev, int type, int flags, int endpt, size_t maxlen);
errorcode_t usbtransfer_schedule(struct USB_TRANSFER* xfer);
void usbtransfer_free(struct USB_TRANSFER* xfer);
void usbtransfer_free_locked(struct USB_TRANSFER* xfer);
void usbtransfer_complete(struct USB_TRANSFER* xfer);
void usbtransfer_complete_locked(struct USB_TRANSFER* xfer);

#endif /* __ANANAS_USB_TRANSFER_H__ */
