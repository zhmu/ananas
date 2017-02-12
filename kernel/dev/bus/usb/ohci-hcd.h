#ifndef __ANANAS_OHCI_HCD_H__
#define __ANANAS_OHCI_HCD_H__

#include <ananas/types.h>
#include <ananas/thread.h>
#include <ananas/dma.h>

#define OHCI_NUM_ED_LISTS 6 /* 1, 2, 4, 8, 16 and 32ms list */

struct OHCI_HCD_TD {
	struct OHCI_TD td_td;
	dma_buf_t td_buf;
	uint32_t td_length;
	LIST_FIELDS(struct OHCI_HCD_TD);
};

struct OHCI_HCD_ED {
	struct OHCI_ED ed_ed;
	/* Virtual addresses of the TD chain */
	struct OHCI_HCD_TD* ed_headtd;
	struct OHCI_HCD_TD* ed_tailtd;
	/* Virtual addresses of the ED chain */
	struct OHCI_HCD_ED* ed_preved;
	struct OHCI_HCD_ED* ed_nexted;
	dma_buf_t ed_buf;
	struct USB_TRANSFER* ed_xfer;
	/* Active queue fields; used by ohci_irq() to check all active transfers */
	LIST_FIELDS_IT(struct OHCI_HCD_ED, active);
};

LIST_DEFINE(OHCI_HCD_ED_QUEUE, struct OHCI_HCD_ED);

struct OHCI_PRIVDATA {
	volatile uint8_t* ohci_membase;
	dma_buf_t ohci_hcca_buf;
	struct OHCI_HCCA* ohci_hcca;
	struct OHCI_HCD_ED* ohci_interrupt_ed[OHCI_NUM_ED_LISTS];
	struct OHCI_HCD_ED* ohci_control_ed;
	struct OHCI_HCD_ED* ohci_bulk_ed;
	struct OHCI_HCD_ED_QUEUE ohci_active_eds;
	int ohci_rh_numports;
	mutex_t ohci_mtx;
	semaphore_t ohci_rh_semaphore;
	thread_t ohci_rh_pollthread;
	struct USB_DEVICE* ohci_roothub;
};

static inline void
ohci_write2(device_t dev, unsigned int reg, uint16_t value)
{
	auto p = static_cast<struct OHCI_PRIVDATA*>(dev->privdata);
	*(volatile uint16_t*)(p->ohci_membase + reg) = value;
}

static inline void
ohci_write4(device_t dev, unsigned int reg, uint32_t value)
{
	auto p = static_cast<struct OHCI_PRIVDATA*>(dev->privdata);
	*(volatile uint32_t*)(p->ohci_membase + reg) = value;
}

static inline uint16_t
ohci_read2(device_t dev, unsigned int reg)
{
	auto p = static_cast<struct OHCI_PRIVDATA*>(dev->privdata);
	return *(volatile uint16_t*)(p->ohci_membase + reg);
}

static inline uint32_t
ohci_read4(device_t dev, unsigned int reg)
{
	auto p = static_cast<struct OHCI_PRIVDATA*>(dev->privdata);
	return *(volatile uint32_t*)(p->ohci_membase + reg);
}

#endif /* __ANANAS_OHCI_HCD_H__ */
