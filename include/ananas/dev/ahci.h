#ifndef __ANANAS_AHCI_H__
#define __ANANAS_AHCI_H__

#include <ananas/types.h>
#include <ananas/device.h>

#define AHCI_DEBUG(fmt, ...) \
	device_printf(dev, fmt, __VA_ARGS__)

struct AHCI_PRIVDATA {
	void* ahci_dev_privdata;
};

#endif /* __ANANAS_AHCI_H__ */
