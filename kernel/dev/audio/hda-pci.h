#ifndef __ANANAS_HDAPCI_H__
#define __ANANAS_HDAPCI_H__

#define HDA_REG_GCAP	0x00
#define  HDA_GCAP_OSS(x)	(((x) >> 12) & 15)	/* output streams supported */
#define  HDA_GCAP_ISS(x)	(((x) >> 8) & 15)	/* input streams supported */
#define  HDA_GCAP_BSS(x)	(((x) >> 3) & 31)	/* bi-directional streams supported */
#define  HDA_GCAP_NSDO(x)	(((x) >> 1) & 3)	/* serial data out signals */
#define  HDA_GCAP_64OK		((x) & 1)		/* 64-bit addresses supported */
#define HDA_REG_VMIN	0x02
#define HDA_REG_VMAJ	0x03
#define HDA_REG_OUTPAY	0x04
#define HDA_REG_INPAY	0x06
#define HDA_REG_GCTL	0x08
#define  HDA_GCTL_UNSOL	(1 << 8)		/* accept unsollicited responses */.
#define  HDA_GCTL_FCNTRL (1 << 1)		/* flush control */
#define  HDA_GCTL_CRST	 (1 << 0)		/* controller reset */
#define HDA_REG_WAKEEN	0x0c
#define HDA_REG_STATESTS	0x0e
#define HDA_REG_GSTS	0x10
#define HDA_REG_OUTSTRMPAY	0x18
#define HDA_REG_INSTRMPAY	0x1a
#define HDA_REG_INTCTL		0x20
#define  HDA_INTCTL_GIE		(1 << 31)	/* global interrupt enable */
#define  HDA_INTCTL_CIE		(1 << 30)	/* controller interrupt enable */
#define  HDA_INTCTL_SIE(x)	(1 << (x))	/* stream interrupt enable */
#define HDA_REG_INTSTS		0x24
#define  HDA_INTSTS_GIS		(1 << 31)	/* global interrupt status */
#define  HDA_INTSTS_CIS		(1 << 30)	/* controller interrupt status */
#define  HDA_INTSTS_SIS(x)	(1 << (x))	/* stream interrupt status */
#define HDA_REG_WALLCLOCK	0x30
#define HDA_REG_SSYNC		0x38
#define HDA_REG_CORBL		0x40
#define HDA_REG_CORBU		0x44
#define HDA_REG_CORBWP		0x48
#define HDA_REG_CORBRP		0x4a
#define HDA_REG_CORBCTL		0x4c
#define  HDA_CORBCTL_CORBRUN	(1 << 1)	/* corb dma engine enable */
#define  HDA_CORBCTL_CMEIE	(1 << 0)	/* corb memory error interrupt enable */
#define HDA_REG_CORBSTS		0x4d
#define  HDA_CORBSTS_CMEI	(1 << 0)	/* corb memory error indication */
#define HDA_REG_CORBSIZE	0x4e
#define  HDA_CORBSIZE_CORBSZCAP(x)	(((x) >> 4) & 15)	/* corb size capability */
#define  HDA_CORBSIZE_CORBSZCAP_2E	(1 << 0)		/* corb size capability: 2 entries */
#define  HDA_CORBSIZE_CORBSZCAP_16E	(1 << 1)		/* corb size capability: 16 entries */
#define  HDA_CORBSIZE_CORBSZCAP_256E	(1 << 2)		/* corb size capability: 256 entries */
#define  HDA_CORBSIZE_CORBSIZE_2E	(0 << 0)		/* corb size: 2 entries */
#define  HDA_CORBSIZE_CORBSIZE_16E	(1 << 0)		/* corb size: 16 entries */
#define  HDA_CORBSIZE_CORBSIZE_256E	(2 << 0)		/* corb size: 256 entries */
#define  HDA_CORBSIZE_CORBSIZE_MASK 	(3 << 0)		/* corb size mask */
#define HDA_REG_RIRBL		0x50
#define HDA_REG_RIRBU		0x54
#define HDA_REG_RIRBWP		0x58
#define  HDA_RIRBWP_RIRBWPRST	(1 << 15)		/* rirb write pointer reset */
#define  HDA_RIRBWP_RIRBWP(x)	((x) & 255)		/* rirb write pointer */
#define HDA_REG_RINTCNT		0x5a
#define HDA_REG_RIRBCTL		0x5c
#define  HDA_RIRBCTL_RIRBOIC	(1 << 2)		/* response overrun interrupt control */
#define  HDA_RIRBCTL_RIRBDMAEN	(1 << 1)		/* rirb dma enable */
#define  HDA_RIRBCTL_RINTCTL	(1 << 0)		/* response interrupt control */
#define HDA_REG_RIRBSTS		0x5d
#define  HDA_RIRBSTS_RIRBOIS	(1 << 2)		/* response overrun interrupt status */
#define  HDA_RIRBSTS_RINTFL	(1 << 0)		/* response interrupt */
#define HDA_REG_RIRBSIZE	0x5e
# define HDA_RIRBSIZE_RIRBSZCAP(x)	(((x) >> 4) & 15)	/* rirb size capability */
# define HDA_RIRBSIZE_RIRBSZCAP_2E	(1 << 0)		/* rirb size capability: 2 entries */
# define HDA_RIRBSIZE_RIRBSZCAP_16E	(1 << 1)		/* rirb size capability: 16 entries */
# define HDA_RIRBSIZE_RIRBSZCAP_256E	(1 << 2)		/* rirb size capability: 256 entries */
# define HDA_RIRBSIZE_RIRBSIZE_2E	(0 << 0)		/* rirb size: 2 entries */
# define HDA_RIRBSIZE_RIRBSIZE_16E	(1 << 0)		/* rirb size: 2 entries */
# define HDA_RIRBSIZE_RIRBSIZE_256E	(2 << 0)		/* rirb size: 2 entries */
# define HDA_RIRBSIZE_RIRBSIZE_MASK 	(3 << 0)
#define HDA_REG_ICW		0x60
#define HDA_REG_IRR		0x64
#define HDA_REG_ICS		0x68
# define HDA_ICS_IRRADD(x)	(((x) >> 4) & 15)	/* immediate response result address */
# define HDA_ICS_IRRUNSOL(x)	(1 << 3)		/* immediate response result unsolicited */
# define HDA_ICS_ICVER		(1 << 2)		/* immediate command version */
# define HDA_ICS_ICV		(1 << 1)		/* immediate command valid */
# define HDA_ICS_ICB		(1 << 0)		/* immediate command busy */
#define HDA_REG_DPLBASE		0x70
# define HDA_DPLBASE_DPLBASE(x)	(((x) >> 7))		/* dma position lower base address */
# define HDA_DPLBASE_DPBE	(0 << 0)		/* dma position buffer enable */
#define HDA_REG_DPUBASE		0x74
#define HDA_REG_xSDnCTL(n)	(0x80 + ((n) * 0x20))	/* {input,output,bidirectional} stream control n */
#define  HDA_SDnCTL_STRM(x)	((x) << 20)		/* stream number */
#define  HDA_SDnCTL_DIR_IN	(0 << 19)
#define  HDA_SDnCTL_DIR_OUT	(1 << 19)
#define  HDA_SDnCTL_TP		(1 << 18)        	/* traffic priority */
#define  HDA_SDnCTL_STRIPE	(1 << 16)       	/* stripe control */
#define  HDA_SDnCTL_DEIE	(1 << 4)         	/* descriptor error interrupt enable */
#define  HDA_SDnCTL_FEIE	(1 << 3)        	/* FIFO error interrupt enable */
#define  HDA_SDnCTL_IOCE	(1 << 2)        	/* interrupt on completion enable */
#define  HDA_SDnCTL_RUN		(1 << 1)		/* stream run */
#define  HDA_SDnCTL_SRST	(1 << 0)		/* stream reset */
#define HDA_REG_xSDnSTS(n)	(HDA_REG_xSDnCTL(n) + 0x03)
#define  HDA_SDnSTS_FIFORDY	(1 << 5)		/* FIFO ready */
#define  HDA_SDnSTS_DESE	(1 << 4)		/* descriptor error */
#define  HDA_SDnSTS_FIFOE	(1 << 3)		/* FIFO error */
#define  HDA_SDnSTS_BCIS	(1 << 2)		/* buffer completion interrupt status */
#define HDA_REG_xSDnLPIB(n)	(HDA_REG_xSDnCTL(n) + 0x04)
#define HDA_REG_xSDnCBL(n)	(HDA_REG_xSDnCTL(n) + 0x08)
#define HDA_REG_xSDnLVI(n)	(HDA_REG_xSDnCTL(n) + 0x0c)
#define HDA_REG_xSDnFIFOS(n)	(HDA_REG_xSDnCTL(n) + 0x10)
#define HDA_REG_xSDnFMT(n)	(HDA_REG_xSDnCTL(n) + 0x12)
# define HDA_SDnFMT_BASE(x)	((x) << 14)
#  define HDA_SDnFMT_BASE_48	0
#  define HDA_SDnFMT_BASE_44_1	1
# define HDA_SDnFMT_MULT(x)	((x) << 11)
# define HDA_SDnFMT_DIV(x)	((x) << 8)
# define HDA_SDnFMT_BITS(x)	((x) << 4)
# define HDA_SDnFMT_CHAN(x)	((x) << 0)
#define HDA_REG_xSDnBDPL(n)	(HDA_REG_xSDnCTL(n) + 0x18)
#define HDA_REG_xSDnBDPU(n)	(HDA_REG_xSDnCTL(n) + 0x1c)

struct HDA_PCI_BDL_ENTRY {
	uint64_t	bdl_addr;
	uint32_t	bdl_length;
	uint32_t	bdl_flags;
#define BDL_FLAG_IOC	1
} __attribute__((packed));

struct HDA_PCI_STREAM;

struct HDA_PCI_PRIVDATA {
};

struct HDA_PCI_STREAM_PAGE {
	struct PAGE* sp_page;
	void* sp_ptr;
};

struct HDA_PCI_STREAM {
	int s_ss;			/* Stream# in use */
	int s_num_pages;
	struct HDA_PCI_BDL_ENTRY* s_bdl;	/* BDL entries */
	struct PAGE* s_bdl_page;	/* Page for the BDL */
	struct HDA_PCI_STREAM_PAGE s_page[0];
};

#endif /* __ANANAS_HDAPCI_H__ */
