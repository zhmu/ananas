#ifndef __ANANAS_USB_TRANSFER_H__
#define __ANANAS_USB_TRANSFER_H__

struct USB_DEVICE;

errorcode_t usb_control_xfer(struct USB_DEVICE* usb_dev, int req, int recipient, int type, int value, int index, void* buf, size_t* len, int write);

#endif /* __ANANAS_USB_TRANSFER_H__ */
