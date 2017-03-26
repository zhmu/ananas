#ifndef __ANANAS_USB_CONFIG_H__
#define __ANANAS_USB_CONFIG_H__

#include <ananas/types.h>

struct USB_DEVICE;

errorcode_t usb_parse_configuration(struct USB_DEVICE* usb_dev, void* data, int datalen);

#endif
