#ifndef __OHCI_REG_H__
#define __OHCI_REG_H__

#define OHCI_HCREVISION		0x00
# define OHCI_REVISION(x)	((x) & 0xff)
#define OHCI_HCCONTROL		0x04
# define OHCI_CONTROL_CBSR(x)	((x) << 0)
#  define OHCI_CBSR_1_1		0
#  define OHCI_CBSR_2_1		1
#  define OHCI_CBSR_3_1		2
#  define OHCI_CBSR_4_1		3
#  define OHCI_CBSR_MASK	3
# define OHCI_CONTROL_CBSR(x)	((x) << 0)
# define OHCI_CONTROL_PLE	(1 << 2)
# define OHCI_CONTROL_IE	(1 << 3)
# define OHCI_CONTROL_CLE	(1 << 4)
# define OHCI_CONTROL_BLE	(1 << 5)
# define OHCI_CONTROL_HCFS(x)	((x) << 6)
#  define OHCI_HCFS_USBRESET		0
#  define OHCI_HCFS_USBRESUME		1
#  define OHCI_HCFS_USBOPERATIONAL	2
#  define OHCI_HCFS_USBSUSPEND		3
#  define OHCI_HCFS_MASK		3
# define OHCI_CONTROL_IR	(1 << 8)
# define OHCI_CONTROL_RWC	(1 << 9)
# define OHCI_CONTROL_RWE	(1 << 10)
#define OHCI_HCCOMMANDSTATUS	0x08
# define OHCI_CS_HCR		(1 << 0)
# define OHCI_CS_CLF		(1 << 1)
# define OHCI_CS_BLF		(1 << 2)
# define OHCI_CS_OCR		(1 << 3)
# define OHCI_CS_SOC		(1 << 16)
#define OHCI_HCINTERRUPTSTATUS	0x0c
# define OHCI_IS_SO		(1 << 0)
# define OHCI_IS_WDH		(1 << 1)
# define OHCI_IS_SF		(1 << 2)
# define OHCI_IS_RD		(1 << 3)
# define OHCI_IS_UE		(1 << 4)
# define OHCI_IS_FNO		(1 << 5)
# define OHCI_IS_RHSC		(1 << 6)
# define OHCI_IS_OC		(1 << 30)
#define OHCI_HCINTERRUPTENABLE	0x10
# define OHCI_IE_SO		(1 << 0)
# define OHCI_IE_WDH		(1 << 1)
# define OHCI_IE_SF		(1 << 2)
# define OHCI_IE_RD		(1 << 3)
# define OHCI_IE_UE		(1 << 4)
# define OHCI_IE_FNO		(1 << 5)
# define OHCI_IE_RHSC		(1 << 6)
# define OHCI_IE_OC		(1 << 30)
# define OHCI_IE_MIE		(1 << 31)
#define OHCI_HCINTERRUPTDISABLE	0x14
# define OHCI_ID_SO		(1 << 0)
# define OHCI_ID_WDH		(1 << 1)
# define OHCI_ID_SF		(1 << 2)
# define OHCI_ID_RD		(1 << 3)
# define OHCI_ID_UE		(1 << 4)
# define OHCI_ID_FNO		(1 << 5)
# define OHCI_ID_RHSC		(1 << 6)
# define OHCI_ID_OC		(1 << 30)
# define OHCI_ID_MIE		(1 << 31)
#define OHCI_HCHCCA		0x18
#define OHCI_HCPERIODCURRENTED	0x1c
#define OHCI_HCCONTROLHEADED	0x20
#define OHCI_HCCONTROLCURRENTED	0x24
#define OHCI_HCBULKHEADED	0x28
#define OHCI_HCBULKCURRENTED	0x2c
#define OHCI_HCDONEHEAD		0x30
#define OHCI_HCFMINTERVAL	0x34
# define OHCI_FM_FI(x)		((x) & 0x3fff)
# define OHCI_FM_FIT		(1 << 31)
# define OHCI_FM_MAX_OVERHEAD	210
# define OHCI_FM_FSMPS(x)	((x) << 16)
#define OHCI_HCFMREMAINING	0x38
#define OHCI_HCFMNUMBER		0x3c
#define OHCI_HCPERIODICSTART	0x40
#define OHCI_HCLSTHRESHOLD	0x44
#define OHCI_HCRHDESCRIPTORA	0x48
# define OHCI_RHDA_NDP(x)	((x) & 0x7f)
# define OHCI_RHDA_PSM		(1 << 8)
# define OHCI_RHDA_NPS		(1 << 9)
# define OHCI_RHDA_DT		(1 << 10)
# define OHCI_RHDA_OCPM		(1 << 11)
# define OHCI_RHDA_NOCP		(1 << 12)
# define OHCI_RHDA_POTPGT(x)	(((x) >> 24) & 0xff)
#define OHCI_HCRHDESCRIPTORB	0x4c
# define OHCI_RHDB_DR(x)	(x)
# define OHCI_RHDB_PPCM(x)	((x) << 16)
#define OHCI_HCRHSTATUS		0x50
# define OHCI_RHS_LPS		(1 << 0)
# define OHCI_RHS_OCI		(1 << 1)
# define OHCI_RHS_DRWE		(1 << 15)
# define OHCI_RHS_LPSC		(1 << 16)
# define OHCI_RHS_OCIC		(1 << 17)
# define OHCI_RHS_CRWE		(1 << 31)
#define OHCI_HCRHPORTSTATUSx	0x54
# define OHCI_RHPS_CCS		(1 << 0)
# define OHCI_RHPS_PES		(1 << 1)
# define OHCI_RHPS_PSS		(1 << 2)
# define OHCI_RHPS_POCI		(1 << 3)
# define OHCI_RHPS_PRS		(1 << 4)
# define OHCI_RHPS_PPS		(1 << 8)
# define OHCI_RHPS_LSDA		(1 << 9)
# define OHCI_RHPS_CSC		(1 << 16)
# define OHCI_RHPS_PESC		(1 << 17)
# define OHCI_RHPS_PSSC		(1 << 18)
# define OHCI_RHPS_OCIC		(1 << 19)
# define OHCI_RHPS_PRSC		(1 << 20)

struct OHCI_ED {
	uint32_t	ed_flags;
#define OHCI_ED_MPS(x)	((x) << 16)
#define OHCI_ED_F		(1 << 15)
#define OHCI_ED_K		(1 << 14)
#define OHCI_ED_S		(1 << 13)
#define OHCI_ED_D(x)	((x) << 11)
# define OHCI_ED_D_TD 0
# define OHCI_ED_D_OUT 1
# define OHCI_ED_D_IN 2
#define OHCI_ED_EN(x)	((x) << 7)
#define OHCI_ED_FA(x)	(x)
	uint32_t	ed_tailp;
	uint32_t	ed_headp;
#define OCHI_ED_HEADP_C		(1 << 1)
#define OCHI_ED_HEADP_H		(1 << 0)
	uint32_t	ed_nexted;
} __attribute__((packed));

struct OHCI_TD {
	uint32_t	td_flags;
#define OHCI_TD_CC(x)		((x) << 28)
#define OHCI_TD_EC(x)		((x) << 26)
#define OHCI_TD_T(x)		((x) << 24)
#define OHCI_TD_DI(x)		((x) << 21)
# define OHCI_TD_DI_IMMEDIATE	0
# define OHCI_TD_DI_NONE	7
# define OHCI_TD_DI_MASK	7
#define OHCI_TD_DP(x)		((x) << 19)
# define OHCI_TD_DP_SETUP 0
# define OHCI_TD_DP_OUT 1
# define OHCI_TD_DP_IN 2
#define OHCI_TD_R		(1 << 18)
	uint32_t	td_cbp;
	uint32_t	td_nexttd;
	uint32_t	td_be;
} __attribute__((packed));

struct OHCI_IT {
	uint32_t	it_flags;
#define OHCI_IT_CC(x)		((x) << 28)
#define OHCI_IT_FC(x)		((x) << 24)
#define OHCI_IT_DI(x)		((x) << 21)
#define OHCI_IT_SF(x)		(x)
	uint32_t	it_bp0;
	uint32_t	it_nexttd;
	uint32_t	it_be;
	uint32_t	it_psw_1_0;
#define OHCI_IT_PSW_CC(x)	((x) >> 12)
#define OHCI_IT_PSW_SIZE(x)	((x) & 0x7ff)
	uint32_t	it_psw_3_2;
	uint32_t	it_psw_5_4;
	uint32_t	it_psw_7_6;
} __attribute__((packed));

struct OHCI_HCCA {
	uint32_t	hcca_inttable[32];
	uint16_t	hcca_framenumber;
	uint16_t	hcca_pad1;
	uint32_t	hcca_donehead;
	uint8_t		hcca_reserved[116];
} __attribute__((packed));

#endif /* __OHCI_REG_H__ */
