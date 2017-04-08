#ifndef __UHCI_HUB_H__
#define __UHCI_HUB_H__

namespace Ananas {
namespace USB {

class Transfer;
class USBDevice;
class UHCI_HCD;

namespace UHCIRootHub {

void Start(UHCI_HCD& hcd, USBDevice& usb_dev);
errorcode_t HandleTransfer(Transfer& xfer);

} // namespace UHCIRootHub
} // namespace USB
} // namespace Ananas

#endif /* __UHCI_HUB_H__ */
