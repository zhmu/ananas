#ifndef __UHCI_HCD_H__
#define __UHCI_HCD_H__

#define UHCI_FRAMELIST_LEN	(4096 / 4)
#define UHCI_NUM_INTERRUPT_QH	6 /* 1, 2, 4, 8, 16, 32ms queues */

LIST_DEFINE(UHCI_SCHEDULED_ITEM_QUEUE, struct UHCI_SCHEDULED_ITEM);
LIST_DEFINE(UHCI_TD_QUEUE, struct UHCI_HCD_TD);
LIST_DEFINE(UHCI_QH_QUEUE, struct UHCI_HCD_QH);

struct UHCI_PRIVDATA {
	/* Mutex used to protect the privdata */
	mutex_t uhci_mtx;

	dma_buf_t uhci_framelist_buf;
	uint32_t* uhci_framelist;

	/* Have we completed a port reset? */
	int uhci_c_portreset;

	/* Number of root hub ports */
	unsigned int uhci_rh_numports;
	thread_t uhci_rh_pollthread;
	struct USB_DEVICE* uhci_roothub;

	/* I/O port */
	uint32_t uhci_io;
	/* Start of frame value */
	uint32_t uhci_sof_modify;
	/* Interrupt/control/bulk QH's */
	struct UHCI_HCD_QH* uhci_qh_interrupt[UHCI_NUM_INTERRUPT_QH];
	struct UHCI_HCD_QH* uhci_qh_ls_control;
	struct UHCI_HCD_QH* uhci_qh_fs_control;
	struct UHCI_HCD_QH* uhci_qh_bulk;
	/* Currently scheduled queue items */
	struct UHCI_SCHEDULED_ITEM_QUEUE uhci_scheduled_items;
};

struct UHCI_DEV_PRIVDATA {
	int dev_flags;
#define UHCI_DEV_FLAG_LOWSPEED	1
};

#endif /* __UHCI_HCD_H__ */
