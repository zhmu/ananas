#ifndef __ANANAS_BUS_USB_TRANSFER_H__
#define __ANANAS_BUS_USB_TRANSFER_H__

namespace Ananas {
namespace USB {

class USBDevice;

errorcode_t usb_control_xfer(USBDevice& usb_dev, int req, int recipient, int type, int value, int index, void* buf, size_t* len, int write);

} // namespace USB
} // namespace Ananas

#endif /* __ANANAS_BUS_USB_TRANSFER_H__ */
