#ifndef __ANANAS_USB_CONFIG_H__
#define __ANANAS_USB_CONFIG_H__

class Result;

namespace usb
{
    class Interface;

    Result ParseConfiguration(Interface* usb_if, int& usb_num_if, void* data, int datalen);

} // namespace usb

#endif
