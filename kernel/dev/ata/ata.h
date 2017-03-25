#include <ananas/queue.h>
#include <ananas/device.h>

#ifndef __ATA_H__
#define __ATA_H__

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

#define ATA_CMD_READ_SECTORS		0x20	/* 28 bit PIO */
#define ATA_CMD_DMA_READ_EXT		0x25	/* 48 bit DMA */
#define ATA_CMD_WRITE_SECTORS		0x30	/* 28 bit PIO */
#define ATA_CMD_DMA_WRITE_EXT		0x35	/* 48 bit DMA */
#define ATA_CMD_PACKET			0xa0
#define ATA_CMD_IDENTIFY_PACKET		0xa1
#define ATA_CMD_READ_MULTIPLE		0xc4	/* 28 bit DMA */
#define ATA_CMD_WRITE_MULTIPLE		0xc5	/* 28 bit PIO */
#define ATA_CMD_DMA_READ_SECTORS	0xc8	/* 28 bit DMA */
#define ATA_CMD_DMA_WRITE_SECTORS	0xca	/* 28 bit DMA */
#define ATA_CMD_IDENTIFY		0xec

#define ATAPI_CMD_READ_CAPACITY		0x25
#define ATAPI_CMD_READ_SECTORS		0x28

struct ATA_IDENTIFY {
	/*   0 */ uint8_t general_cfg[2];
#define ATA_GENCFG_NONATA	(1 << 15)
#define ATA_GENCFG_NONATAPI	(1 << 14)
#define ATA_GENCFG_REMOVABLE	(1 << 7)
/* Bits 8-12 define the device type */
#define ATA_TYPE_DIRECTACCESS	0x00
#define ATA_TYPE_SEQACCESS	0x01
#define ATA_TYPE_PRINTER	0x02
#define ATA_TYPE_PROCESSOR	0x03
#define ATA_TYPE_WRITEONCE	0x04
#define ATA_TYPE_CDROM		0x05
#define ATA_TYPE_SCANNER	0x06
#define ATA_TYPE_OPTICALMEM	0x07
#define ATA_TYPE_CHANGER	0x08
#define ATA_TYPE_COMMS		0x09
#define ATA_TYPE_ARRAY		0x0c
#define ATA_TYPE_ENCLOSURE	0x0d
#define ATA_TYPE_REDUCEDBLK	0x0e
#define ATA_TYPE_OPTICARD	0x0f
#define ATA_TYPE_UNKNOWN	0x1f
	/*   1 */ uint8_t num_cyl[2];
	/*   2 */ uint8_t reserved0[2];
	/*   3 */ uint8_t num_heads[2];
	/*   4 */ uint8_t vendor0[4];
	/*   6 */ uint8_t num_sectors[2];
	/*   7 */ uint8_t vendor1[6];
	/*  10 */ uint8_t serial[20];
	/*  20 */ uint8_t vendor2[2];
	/*  21 */ uint8_t buf_size[2];
	/*  22 */ uint8_t vendor3[2];
	/*  23 */ uint8_t firmware[8];
	/*  27 */ uint8_t model[40];
	/*  47 */ uint8_t rw_multi_support[2];
	/*  48 */ uint8_t dword_xfers[2];
	/*  49 */ uint8_t capabilities1[2];
#define ATA_CAP_LBA		(1 << 9)
#define ATA_CAP_DMA		(1 << 8)
	/*  50 */ uint8_t capabilities2[2];
	/*  51 */ uint8_t pio_timing[2];
	/*  52 */ uint8_t dma_timing[2];
	/*  53 */ uint8_t validity[2];
	/*  54 */ uint8_t cur_cyl[2];
	/*  55 */ uint8_t cur_heads[2];
	/*  56 */ uint8_t cur_sectors[2];
	/*  57 */ uint8_t cur_capacity[4];
	/*  59 */ uint8_t multi_sector[2];
	/*  60 */ uint8_t lba_sectors[4];
	/*  62 */ uint8_t single_dma_modes[2];
	/*  63 */ uint8_t multi_dma_modes[2];
#define ATA_MDMA_SEL_MWDMA2	(1 << 10)
#define ATA_MDMA_SEL_MWDMA1	(1 << 9)
#define ATA_MDMA_SEL_MWDMA0	(1 << 8)
#define ATA_MDMA_SUPPORT_MWDMA2	(1 << 2)
#define ATA_MDMA_SUPPORT_MWDMA1	(1 << 1)
#define ATA_MDMA_SUPPORT_MWDMA0	(1 << 0)
	/*  64 */ uint8_t pio_supported[2];
	/*  65 */ uint8_t min_dma_cycle[2];
	/*  66 */ uint8_t rec_multidma_cycle[2];
	/*  67 */ uint8_t min_noflow_control[2];
	/*  68 */ uint8_t min_pio_xfer[2];
	/*  69 */ uint8_t reserved1[12];
	/*  75 */ uint8_t queue_depth[2];
	/*  76 */ uint8_t reserved2[8];
	/*  80 */ uint8_t major_ata_spec[2];
#define ATA_SPEC_ATAPI14	(1 << 14)
#define ATA_SPEC_ATAPI13	(1 << 13)
#define ATA_SPEC_ATAPI12	(1 << 12)
#define ATA_SPEC_ATAPI11	(1 << 11)
#define ATA_SPEC_ATAPI10	(1 << 10)
#define ATA_SPEC_ATAPI9		(1 <<  9)
#define ATA_SPEC_ATAPI8		(1 <<  8)
#define ATA_SPEC_ATAPI7		(1 <<  7)
#define ATA_SPEC_ATAPI6		(1 <<  6)
#define ATA_SPEC_ATAPI5		(1 <<  5)
#define ATA_SPEC_ATAPI4		(1 <<  4)
	/*  81 */ uint8_t minor_ata_spec[2];
	/*  82 */ uint8_t features1[2];
#define ATA_FEAT1_NOP		(1 <<  14)
#define ATA_FEAT1_READBUFFER	(1 <<  13)
#define ATA_FEAT1_WRITEBUFFER	(1 <<  12)
#define ATA_FEAT1_HOSTPROTAREA	(1 <<  10)
#define ATA_FEAT1_DEVICERESET	(1 <<  9)
#define ATA_FEAT1_SERVICEINT	(1 <<  8)
#define ATA_FEAT1_RELEASEINT	(1 <<  7)
#define ATA_FEAT1_LOOKAHEAD	(1 <<  6)
#define ATA_FEAT1_WCACHE	(1 <<  5)
#define ATA_FEAT1_PACKET	(1 <<  4)
#define ATA_FEAT1_PM		(1 <<  3)
#define ATA_FEAT1_REMOVABLE	(1 <<  2)
#define ATA_FEAT1_SECURITY	(1 <<  1)
#define ATA_FEAT1_SMART		(1 <<  0)
	/*  83 */ uint8_t features2[2];
#define ATA_FEAT2_FLCACHE_EXT	(1 << 13)
#define ATA_FEAT2_FLCACHE	(1 << 12)
#define ATA_FEAT2_DEV_CO	(1 << 11)
#define ATA_FEAT2_LBA48		(1 << 10)
#define ATA_FEAT2_AAM		(1 <<  9)
#define ATA_FEAT2_SET_MAX_SEC	(1 <<  8)
#define ATA_FEAT2_NEED_SETFEAT	(1 <<  6)
#define ATA_FEAT2_PU_STANDBY	(1 <<  5)
#define ATA_FEAT2_RM_SUPPORTED	(1 <<  4)
#define ATA_FEAT2_ADV_PM	(1 <<  3)
#define ATA_FEAT2_CFA_SUPPORTED	(1 <<  2)
#define ATA_FEAT2_RWDMAQUEUD	(1 <<  1)
#define ATA_FEAT2_DOWNLOADMCODE	(1 <<  0)
	/*  84 */ uint8_t features_ext[2];
#define ATA_FEXT_IDLE_IMM	(1 << 13)
#define ATA_FEXT_URG_WRITE	(1 << 10)
#define ATA_FEXT_URG_READ	(1 <<  9)
#define ATA_FEXT_64BIT_WWN	(1 <<  8)
#define ATA_FEXT_WRQUEUFUAEXT	(1 <<  7)
#define ATA_FEXT_WRFUAEXT	(1 <<  6)
#define ATA_FEXT_GP_LOG		(1 <<  5)
#define ATA_FEXT_STREAMING	(1 <<  4)
#define ATA_FEXT_MC_PASSTHROUGH	(1 <<  3)
#define ATA_FEXT_MEDIA_SERNUM	(1 <<  2)
#define ATA_FEXT_SMART_SELFTEST	(1 <<  1)
#define ATA_FEXT_SMART_ERRORLOG	(1 <<  0)
	/*  85 */ uint8_t features1_enabled[2];
#define ATA_FE1_NOP		(1 << 14)
#define ATA_FE1_READBUFFER	(1 << 13)
#define ATA_FE1_WRITEBUFFER	(1 << 12)
#define ATA_FE1_HOST_PROTAREA	(1 << 10)
#define ATA_FE1_DEVICE_RESET	(1 <<  9)
#define ATA_FE1_SERVICE_INT	(1 <<  8)
#define ATA_FE1_RELEASE_INT	(1 <<  7)
#define ATA_FE1_LOOKAHEAD	(1 <<  6)
#define ATA_FE1_WRITE_CACHE	(1 <<  5)
#define ATA_FE1_PACKET		(1 <<  4)
#define ATA_FE1_PM		(1 <<  3)
#define ATA_FE1_RM		(1 <<  2)
#define ATA_FE1_SECURITY	(1 <<  1)
#define ATA_FE1_SMART		(1 <<  0)
	/*  86 */ uint8_t features2_enabled[2];
#define ATA_FE2_FLUSHCACHE_EXT	(1 << 13)
#define ATA_FE2_FLUSHCACHE	(1 << 12)
#define ATA_FE2_DEVCONF_OVERLAY	(1 << 11)
#define ATA_FE2_LBA48		(1 << 10)
#define ATA_FE2_AAM		(1 <<  9)
#define ATA_FE2_SET_MAX		(1 <<  8)
#define ATA_FE2_NEED_SET_FEAT	(1 <<  6)
#define ATA_FE2_PU_STANDBY	(1 <<  5)
#define ATA_FE2_RM_NOTIFY	(1 <<  4)
#define ATA_FE2_ADV_PM		(1 <<  3)
#define ATA_FE2_CFA		(1 <<  2)
#define ATA_FE2_RW_DMA_QUEUED	(1 <<  1)
#define ATA_FE2_DL_MICROCODE	(1 <<  0)
	/*  87 */ uint8_t features_ext_enabled[2];
#define ATA_FEE_IDLE_IMM	(1 << 13)
#define ATA_FEE_WR_STREAM_URG	(1 << 10)
#define ATA_FEE_RD_STREAM_URG	(1 <<  9)
#define ATA_FEE_64BIT_WWN	(1 <<  8)
#define ATA_FEE_WRDMAQUEUED_FUA	(1 <<  7)
#define ATA_FEE_WRDMA_FUA	(1 <<  6)
#define ATA_FEE_GP_LOG		(1 <<  5)
#define ATA_FEE_CONF_STREAM	(1 <<  4)
#define ATA_FEE_MC_PASSTHRU	(1 <<  3) 
#define ATA_FEE_MEDIA_SERNUM	(1 <<  2)
#define ATA_FEE_SMART_SELFTEST	(1 <<  1)
#define ATA_FEE_SMART_ERROLOG	(1 <<  0)
	/*  88 */ uint8_t udma_selected[2];
#define ATA_UDMASEL_UDMA6	(1 << 14)
#define ATA_UDMASEL_UDMA5	(1 << 13)
#define ATA_UDMASEL_UDMA4	(1 << 12)
#define ATA_UDMASEL_UDMA3	(1 << 11)
#define ATA_UDMASEL_UDMA2	(1 << 10)
#define ATA_UDMASEL_UDMA1	(1 <<  9)
#define ATA_UDMASEL_UDMA0	(1 <<  8)
#define ATA_UDMASEL_UDMA6_SUP	(1 <<  6)
#define ATA_UDMASEL_UDMA5_SUP	(1 <<  5)
#define ATA_UDMASEL_UDMA4_SUP	(1 <<  4)
#define ATA_UDMASEL_UDMA3_SUP	(1 <<  3)
#define ATA_UDMASEL_UDMA2_SUP	(1 <<  2)
#define ATA_UDMASEL_UDMA1_SUP	(1 <<  1)
#define ATA_UDMASEL_UDMA0_SUP	(1 <<  0)
	/*  89 */ uint8_t secure_erase_time[2];
	/*  90 */ uint8_t enhanced_secure_erase_time[2];
	/*  91 */ uint8_t cur_adv_pm[2];
	/*  92 */ uint8_t master_pw_revcode[2];
	/*  93 */ uint8_t hw_reset[2];
	/*  94 */ uint8_t acoustic_val[2];
	/*  95 */ uint8_t stream_min_reqsize[2];
	/*  96 */ uint8_t stream_dma_time[2];
	/*  97 */ uint8_t stream_latency[2];
	/*  98 */ uint8_t stream_perf_granularity[4];
	/* 100 */ uint8_t lba48_sectors[8];
	/* 104 */ uint8_t stream_xfer_time_pio[2];
	/* 105 */ uint8_t reserved3[2];
	/* 106 */ uint8_t sector_size[2];
	/* 107 */ uint8_t int_seek_delay[2];
	/* 108 */ uint8_t ieee_oui23_12[2];
	/* 109 */ uint8_t ieee_oui11_0[2];
	/* 110 */ uint8_t unq_id_31_16[2];
	/* 111 */ uint8_t unq_id_15_0[2];
	/* 112 */ uint8_t reserved4[10];
	/* 117 */ uint8_t words_per_sector[4];
	/* 119 */ uint8_t reserved5[16];
	/* 127 */ uint8_t rmsn_support[2];
	/* 128 */ uint8_t security_status[2];
	/* 129 */ uint8_t vendor4[62];
	/* 160 */ uint8_t cfa_pm_1[2];
	/* 161 */ uint8_t reserved7[30];
	/* 176 */ uint8_t media_serialnum[60];
	/* 206 */ uint8_t reserved8[98];
	/* 255 */ uint8_t checksum[2];
} __attribute__((packed));

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

#define ATA_GET_WORD(x) \
	((uint16_t)(x)[0] << 8 | (x)[1])
#define ATA_GET_DWORD(x) \
	(((uint32_t)ATA_GET_WORD(x+2) << 16) | \
	 ((uint32_t)(ATA_GET_WORD(x))))
#define ATA_GET_QWORD(x) \
	(((uint64_t)ATA_GET_DWORD(x+4) << 32) | \
	((uint64_t)(ATA_GET_DWORD(x))))
	
#define SECTOR_SIZE 512 /* XXX */


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
