#ifndef __OHCI_ROOTHUB_H__
#define __OHCI_ROOTHUB_H__

#include <ananas/device.h>
#include <ananas/thread.h>

errorcode_t ohci_roothub_create(device_t ohci_dev);
void ohci_roothub_irq(device_t dev);

#endif /* __OHCI_ROOTHUB_H__ */
