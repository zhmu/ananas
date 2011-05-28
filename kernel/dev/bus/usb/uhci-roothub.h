#ifndef __UHCI_HUB_H__
#define __UHCI_HUB_H__

#include <ananas/device.h>
#include <ananas/thread.h>

struct UHCI_HUB_PRIVDATA {
	device_t	hub_uhcidev;          /* UHCI device we belong to */
	int		hub_numports;         /* Number of UHCI ports managed */
	device_t	hub_dev;              /* Our own device */
	thread_t	hub_pollthread;       /* Kernel thread used to monitor status */
	int		hub_flags;            /* How are we doing? */
#define HUB_FLAG_ATTACHING	(1 << 0)     /* Device attachment in progress */
};


errorcode_t uhci_hub_create(device_t uhci_dev, int portno);

#endif /* __UHCI_HUB_H__ */
