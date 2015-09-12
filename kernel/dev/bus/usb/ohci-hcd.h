#ifndef __ANANAS_OHCI_HCD_H__
#define __ANANAS_OHCI_HCD_H__

#include <ananas/types.h>
#include <ananas/dma.h>

#define OHCI_NUM_ED_LISTS 6 /* 1, 2, 4, 8, 16 and 32ms list */

struct OHCI_HCD_TD {
	struct OHCI_TD td_td;
	dma_buf_t td_buf;
	DQUEUE_FIELDS(struct OHCI_HCD_TD);
};

struct OHCI_HCD_ED {
	struct OHCI_ED ed_ed;
	dma_buf_t ed_buf;
	DQUEUE_FIELDS(struct OHCI_HCD_ED);
};

struct OHCI_HCD_SCHEDULED_ITEM {
	struct OHCI_HCD_TD* si_firsttd;
	struct USB_TRANSFER* si_xfer;
	DQUEUE_FIELDS(struct OHCI_HCD_SCHEDULED_ITEM);
};

DQUEUE_DEFINE(OHCI_SCHEDULED_ITEM_QUEUE, struct OHCI_HCD_SCHEDULED_ITEM);

struct OHCI_PRIVDATA {
	volatile uint8_t* ohci_membase;
	dma_buf_t ohci_hcca_buf;
	device_t ohci_roothub;
	struct OHCI_HCCA* ohci_hcca;
	struct OHCI_HCD_ED* ohci_interrupt_ed[OHCI_NUM_ED_LISTS];
	struct OHCI_HCD_ED* ohci_control_ed;
	struct OHCI_HCD_ED* ohci_bulk_ed;
	struct OHCI_SCHEDULED_ITEM_QUEUE ohci_scheduled_items;
};

static inline void
ohci_write2(device_t dev, unsigned int reg, uint16_t value)
{
	struct OHCI_PRIVDATA* p = dev->privdata;
	*(volatile uint16_t*)(p->ohci_membase + reg) = value;
}

static inline void
ohci_write4(device_t dev, unsigned int reg, uint32_t value)
{
	struct OHCI_PRIVDATA* p = dev->privdata;
	*(volatile uint32_t*)(p->ohci_membase + reg) = value;
}

static inline uint16_t
ohci_read2(device_t dev, unsigned int reg)
{
	struct OHCI_PRIVDATA* p = dev->privdata;
	return *(volatile uint16_t*)(p->ohci_membase + reg);
}

static inline uint32_t
ohci_read4(device_t dev, unsigned int reg)
{
	struct OHCI_PRIVDATA* p = dev->privdata;
	return *(volatile uint32_t*)(p->ohci_membase + reg);
}

#define OHCI_DEV_FLAG_LOWSPEED 1
#define OHCI_DEV_FLAG_ROOTHUB 2

void* ohci_device_init_privdata(int flags);

#endif /* __ANANAS_OHCI_HCD_H__ */
