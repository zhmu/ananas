#ifndef __ANANAS_USB_PIPE_H__
#define __ANANAS_USB_PIPE_H__

#include <ananas/list.h>

namespace Ananas {
namespace USB {

class USBDevice;
class Endpoint;
class Pipe;
class Transfer;

class IPipeCallback
{
public:
	virtual void OnPipeCallback(Pipe&) = 0;
};

class Pipe {
public:
	Pipe(USBDevice& usb_dev, Endpoint& ep, IPipeCallback& callback)
	 : p_dev(usb_dev), p_ep(ep), p_callback(callback)
	{
	}

	Pipe(const Pipe&) = delete;
	Pipe& operator=(const Pipe&) = delete;

	USBDevice& p_dev;
	Endpoint& p_ep;
	IPipeCallback& p_callback;
	Transfer* p_xfer = nullptr;
	
	/* Provide queue structure */
	LIST_FIELDS(Pipe);
};

LIST_DEFINE(Pipes, Pipe);

errorcode_t AllocatePipe(USBDevice& usb_dev, int num, int type, int dir, size_t maxlen, IPipeCallback& callback, Pipe*& pipe);
void FreePipe(Pipe& pipe);

void FreePipe_Locked(Pipe& pipe);
errorcode_t SchedulePipe(Pipe& pipe);

} // namespace USB
} // namespace Ananas

#endif /* __ANANAS_USB_PIPE_H__ */
