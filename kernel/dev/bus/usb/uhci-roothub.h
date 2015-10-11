#ifndef __UHCI_HUB_H__
#define __UHCI_HUB_H__

#include <ananas/device.h>
#include <ananas/thread.h>

void uroothub_start(device_t dev);
errorcode_t uroothub_handle_transfer(device_t dev, struct USB_TRANSFER* xfer);

#endif /* __UHCI_HUB_H__ */
