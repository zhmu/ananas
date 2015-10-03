#ifndef __ANANAS_USB_HUB_H__
#define __ANANAS_USB_HUB_H__

struct HUB_STATUS {
	uint16_t	hs_hubstatus;
#define HUB_STATUS_POWERLOST		(1 << 0)		/* Local power supply lost */
#define HUB_STATUS_OVERCURRENT		(1 << 1)		/* Over-current condition active */
	uint16_t	hs_hubchange;
#define HUB_CHANGE_LOST			(1 << 0)		/* Local power condition changed */
#define HUB_CHANGE_OVERCURRENT		(1 << 1)		/* Over-current condition changed */
};

struct HUB_PORT_STATUS {
	uint16_t	ps_portstatus;
#define HUB_PORTSTATUS_CONNECTED	(1 << 0)		/* Device is present on this port */
#define HUB_PORTSTATUS_ENABLED		(1 << 1)		/* Port is enabled */
#define HUB_PORTSTATUS_SUSPENDED	(1 << 2)		/* Port is suspended or resuming */
#define HUB_PORTSTATUS_OVERCURRENT	(1 << 3)		/* Over-current condition present */
#define HUB_PORTSTATUS_RESET		(1 << 4)		/* Port is resetting */
#define HUB_PORTSTATUS_POWERED		(1 << 8)		/* Port is powered */
#define HUB_PORTSTATUS_LOWSPEED		(1 << 9)		/* Low speed device attached */
	uint16_t	ps_portchange;
#define HUB_PORTCHANGE_CONNECT		(1 << 0)		/* Connection state has changed */
#define HUB_PORTCHANGE_ENABLE		(1 << 1)		/* Enable state has changed */
#define HUB_PORTCHANGE_SUSPEND		(1 << 2)		/* Resume complete */
#define HUB_PORTCHANGE_OVERCURRENT	(1 << 3)		/* Over-current has changed */
#define HUB_PORTCHANGE_RESET		(1 << 4)		/* Reset complete */
};

#endif /* __ANANAS_USB_HUB_H__ */
