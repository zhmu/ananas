#ifndef __ATA_H__
#define __ATA_H__

#include "kernel/queue.h"

#define ATA_REG_DATA		0		/* Data Register (R/W) */
#define ATA_REG_ERROR		1		/* Error Register (R) */
#define ATA_REG_FEATURES	1		/* Features Register (W) */
#define ATA_REG_SECTORCOUNT	2		/* Sector count (W) */
#define ATA_REG_SECTORNUM	3		/* Sector Number (R/W) */
#define ATA_REG_CYL_LO		4		/* Cylinder Low Register (R/W) */
#define ATA_REG_CYL_HI		5		/* Cylinder High Register (R/W) */
#define ATA_REG_DEVICEHEAD	6		/* Device/Head Register (R/W) */
#define ATA_REG_STATUS		7		/* Status Register (R) */
 #define ATA_STAT_ERR		(1 << 0)	/* Error */
 #define ATA_STAT_DRQ		(1 << 3)	/* Data Request */
 #define ATA_STAT_DRDY		(1 << 6)	/* Device Ready */
 #define ATA_STAT_BSY		(1 << 7)	/* Busy */
#define ATA_REG_COMMAND		7		/* Command Register (W) */

#define ATA_REG_DEVCONTROL	2		/* Device Control Register (W) */
 #define ATA_DCR_nIEN		(1 << 1)	/* Interrupt Enable Bit */
 #define ATA_DCR_SRST		(1 << 2)	/* Software Reset Bit */
#define ATA_REG_ALTSTATUS	2		/* Alternate Status Register (R) */
#define ATA_REG_DEVADDR		3		/* Device Address Register (R) */

struct ATA_REQUEST_ITEM {
	uint8_t		command;	/* command */
	uint8_t		unit;		/* unit (0=master, 1=slave) */
	uint16_t		count;	/* amount of sectors or bytes */
	uint32_t	flags;		/* request flags */
#define ATA_ITEM_FLAG_READ	(1 << 0)	/* Read */
#define ATA_ITEM_FLAG_WRITE	(1 << 1)	/* Write */
#define ATA_ITEM_FLAG_ATAPI	(1 << 2)	/* ATAPI request */
#define ATA_ITEM_FLAG_DMA	(1 << 3)	/* Use DMA */
	uint64_t	lba;		/* start LBA */
	struct BIO*	bio;		/* associated I/O buffer */
	/* if command = ATA_CMD_PACKET, this is an ATAPI command and we need to send 6 command words */
	uint8_t		atapi_command[12];
	QUEUE_FIELDS(struct ATA_REQUEST_ITEM);
};

QUEUE_DEFINE(ATA_REQUEST_QUEUE, struct ATA_REQUEST_ITEM)


/* ATA PCI registers, primary channel */
#define ATA_PCI_REG_PRI_COMMAND	0x00
 #define ATA_PCI_CMD_START	(1 << 0)		/* Start/stop bit */
 #define ATA_PCI_CMD_RW		(1 << 3)		/* Read/write bit */
#define ATA_PCI_REG_PRI_STATUS	0x02
 #define ATA_PCI_STAT_SIMPLEX	(1 << 7)		/* Simplex operation */
 #define ATA_PCI_STAT_DMA1	(1 << 6)		/* Drive 1 is DMA capable */
 #define ATA_PCI_STAT_DMA0	(1 << 5)		/* Drive 0 is DMA capable */
 #define ATA_PCI_STAT_IRQ	(1 << 2)		/* IRQ triggered for ATA */
 #define ATA_PCI_STAT_ERROR	(1 << 1)		/* Transfer error */
 #define ATA_PCI_STAT_ACTIVE	(1 << 0)		/* Bus in DMA mode */
#define ATA_PCI_REG_PRI_PRDT	0x04

/* ATA PCI registers, secondary channel */
#define ATA_PCI_REG_SEC_COMMAND	0x08
#define ATA_PCI_REG_SEC_STATUS	0x0a
#define ATA_PCI_REG_SEC_PRDT	0x0c

struct ATAPCI_PRDT {
	uint32_t prdt_base;				/* Memory region base address (physical) */
	uint32_t prdt_size;				/* Byte count */
#define ATA_PRDT_EOT		(1 << 31)		/* End Of Transfer */
} __attribute__((packed));

#define ATA_PCI_NUMPRDT		8

#endif /* __ATA_H__ */
