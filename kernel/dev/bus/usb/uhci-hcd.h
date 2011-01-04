#ifndef __UHCI_HCD_H__
#define __UHCI_HCD_H__

#define UHCI_FRAMELIST_LEN	(4096 / 4)
#define UHCI_NUM_TDS		2048
#define UHCI_NUM_QHS		16


struct UHCI_SCHEDULED_ITEM {
	struct UHCI_TD*      si_td;
	struct USB_TRANSFER* si_xfer;
	DQUEUE_FIELDS(struct UHCI_SCHEDULED_ITEM);
};

DQUEUE_DEFINE(UHCI_SCHEDULED_ITEM_QUEUE, struct UHCI_SCHEDULED_ITEM);

struct UHCI_HCD_PRIVDATA {
	/* uhci_framelist must be first, as we feed it directly to the controller */
	uint32_t uhci_framelist[UHCI_FRAMELIST_LEN];
	/* I/O port */
	uint32_t uhci_io;
	/* Start of frame value */
	uint32_t uhci_sof_modify;
	/* TD/QH freelist; they are chained so we just use their pointers */
	struct UHCI_TD* uhci_td_freelist;
	struct UHCI_QH* uhci_qh_freelist;
	/* Interrupt/control/bulk QH's */
	struct UHCI_QH* uhci_qh_interrupt;
	struct UHCI_QH* uhci_qh_control;
	struct UHCI_QH* uhci_qh_bulk;
	/* Currently scheduled queue items */
	struct UHCI_SCHEDULED_ITEM_QUEUE uhci_scheduled_items;
};

struct UHCI_DEV_PRIVDATA {
	int dev_flags;
#define UHCI_DEV_FLAG_LOWSPEED	1
};

void* uhci_device_init_privdata(int ls);
				
#endif /* __UHCI_HCD_H__ */
