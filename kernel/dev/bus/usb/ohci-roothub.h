#ifndef __OHCI_ROOTHUB_H__
#define __OHCI_ROOTHUB_H__

#include <ananas/device.h>

struct USB_TRANSFER;

errorcode_t oroothub_init(device_t dev);
errorcode_t oroothub_handle_transfer(device_t dev, struct USB_TRANSFER* xfer);
void ohci_roothub_irq(device_t dev);

#endif /* __OHCI_ROOTHUB_H__ */
