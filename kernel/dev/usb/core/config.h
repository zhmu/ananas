#ifndef __ANANAS_USB_CONFIG_H__
#define __ANANAS_USB_CONFIG_H__

namespace Ananas {
namespace USB {

class Interface;

errorcode_t ParseConfiguration(Interface* usb_if, int& usb_num_if, void* data, int datalen);

} // namespace USB
} // namespace Ananas

#endif
