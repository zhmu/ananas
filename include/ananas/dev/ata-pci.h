#ifndef __ATAPCI_H__
#define __ATAPCI_H__

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

struct ATAPCI_PRIVDATA {
	uint32_t		atapci_io;
	uint32_t		atapci_prdt_mask;
	struct ATAPCI_PRDT	atapci_prdt[ATA_PCI_NUMPRDT] __attribute__((aligned(4)));
};

#endif /* __ATAPCI_H__ */
