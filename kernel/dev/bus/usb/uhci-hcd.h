#ifndef __UHCI_HCD_H__
#define __UHCI_HCD_H__

#define UHCI_FRAMELIST_LEN	(4096 / 4)

DQUEUE_DEFINE(UHCI_SCHEDULED_ITEM_QUEUE, struct UHCI_SCHEDULED_ITEM);
DQUEUE_DEFINE(UHCI_TD_QUEUE, struct UHCI_HCD_TD);
DQUEUE_DEFINE(UHCI_QH_QUEUE, struct UHCI_HCD_QH);

struct UHCI_HCD_PRIVDATA {
	/* Mutex used to protect the privdata */
	mutex_t uhci_mtx;

	dma_buf_t uhci_framelist_buf;
	uint32_t* uhci_framelist;

	struct UHCI_TD_QUEUE uhci_td_freelist;
	struct UHCI_QH_QUEUE uhci_qh_freelist;

	/* I/O port */
	uint32_t uhci_io;
	/* Start of frame value */
	uint32_t uhci_sof_modify;
	/* Interrupt/control/bulk QH's */
	struct UHCI_HCD_QH* uhci_qh_interrupt;
	struct UHCI_HCD_QH* uhci_qh_control;
	struct UHCI_HCD_QH* uhci_qh_bulk;
	/* Currently scheduled queue items */
	struct UHCI_SCHEDULED_ITEM_QUEUE uhci_scheduled_items;
};

struct UHCI_DEV_PRIVDATA {
	int dev_flags;
#define UHCI_DEV_FLAG_LOWSPEED	1
};

void* uhci_device_init_privdata(int ls);
				
#endif /* __UHCI_HCD_H__ */
