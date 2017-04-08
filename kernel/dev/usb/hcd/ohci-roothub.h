#ifndef __OHCI_ROOTHUB_H__
#define __OHCI_ROOTHUB_H__

#include <ananas/device.h>

namespace Ananas {
namespace USB {

class Transfer;
class USBDevice;
class OHCI_HCD;

namespace OHCIRootHub {

errorcode_t Initialize(OHCI_HCD& hcd);
void Start(OHCI_HCD& hcd, USBDevice& usb_dev);
void OnIRQ(OHCI_HCD& hcd);
errorcode_t HandleTransfer(Transfer& xfer);

} // namespace UHCIRootHub
} // namespace USB
} // namespace Ananas

#endif /* __OHCI_ROOTHUB_H__ */
