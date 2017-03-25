#ifndef __ANANAS_AHCIPCI_H__
#define __ANANAS_AHCIPCI_H__

#include <ananas/types.h>

namespace Ananas {
namespace AHCI {

#define AHCI_REG_CAP	0x00			/* HBA capabilities */
#define  AHCI_CAP_S64A		(1 << 31)
#define  AHCI_CAP_SNCQ		(1 << 30)
#define  AHCI_CAP_SSNTF		(1 << 29)
#define  AHCI_CAP_SMPS		(1 << 28)
#define  AHCI_CAP_SSS		(1 << 27)
#define  AHCI_CAP_SALP		(1 << 26)
#define  AHCI_CAP_SAL		(1 << 25)
#define  AHCI_CAP_SCLO		(1 << 24)
#define  AHCI_CAP_ISS(x)	(((x) >> 20) & 0xf)
#define  AHCI_CAP_SAM		(1 << 18)
#define  AHCI_CAP_SPM		(1 << 17)
#define  AHCI_CAP_FBSS		(1 << 16)
#define  AHCI_CAP_PMD		(1 << 15)
#define  AHCI_CAP_SSC		(1 << 14)
#define  AHCI_CAP_PSC		(1 << 13)
#define  AHCI_CAP_NCS(x)	(((x) >> 8) & 0x1f)
#define  AHCI_CAP_CCCS		(1 << 7)
#define  AHCI_CAP_EMS		(1 << 6)
#define  AHCI_CAP_SXS		(1 << 5)
#define  AHCI_CAP_NP(x)		((x) & 0x1f)
#define AHCI_REG_GHC	0x04			/* Global HBA control */
#define  AHCI_GHC_AE		(1 << 31)
#define  AHCI_GHC_MRSM		(1 << 2)
#define  AHCI_GHC_IE		(1 << 1)
#define  AHCI_GHC_HR		(1 << 0)
#define AHCI_REG_IS		0x08		/* Interrupt Status Register */
#define  AHCI_IS_IPS(x)		(1 << (x))
#define AHCI_REG_PI		0x0c		/* Ports Implemented */
#define  AHCI_PI_PI(x)		(1 << (x))
#define AHCI_REG_VS		0x10		/* AHCI version */
#define  AHCI_VS_MJR		(((x) >> 16) & 0xffff)
#define  AHCI_VS_MNR		((x) & 0xffff)
#define AHCI_REG_CCC_CTL	0x14		/* Command Completion Coalescing Control */
#define  AHCI_CCC_CTL_TV	(1 << 31)
#define  AHCI_CCC_CTL_CC(x)	(((x) & 0xff) << 8)
#define  AHCI_CCC_CTL_INT(x)	(((x) >> 3) & 0x1f)
#define  AHCI_CCC_CTL_EN	(1 << 0)
#define AHCI_REG_CCC_PORTS	0x18		/* Command Command Coalescing Ports */
#define  AHCI_CCC_PORTS_PRT(x)	((1 >> (x)) & 1)
#define AHCI_REG_EM_LOC		0x1c		//* Enclosure Management Location */
#define  AHCI_EM_LOC_OFST(x)	(((x) >> 16) & 0xffff)
#define  AHCI_EM_LOC_SZ(x)	((x) & 0xffff)
#define AHCI_REG_EM_CTL		0x20		/* Enclosure Management Control */
#define  AHCI_EM_CTL_ATTR_PM	(1 << 27)
#define  AHCI_EM_CTL_ATTR_LHD	(1 << 26)
#define  AHCI_EM_CTL_ATTR_XMT	(1 << 25)
#define  AHCI_EM_CTL_ATTR_SMB	(1 << 24)
#define  AHCI_EM_CTL_SUPP_SGPIO	(1 << 19)
#define  AHCI_EM_CTL_SUPP_SES2	(1 << 18)
#define  AHCI_EM_CTL_SUPP_SAFTE	(1 << 17)
#define  AHCI_EM_CTL_SUPP_LED	(1 << 16)
#define  AHCI_EM_CTL_CTL_RST	(1 << 9)
#define  AHCI_EM_CTL_CTL_TM	(1 << 8)
#define  AHCI_EM_CTL_STS_MR	(1 << 0)
#define AHCI_REG_CAP2		0x24		/* HBA Capabilities Extended */
#define  AHCI_CAP2_DESO		(1 << 5)
#define  AHCI_CAP2_SADM		(1 << 4)
#define  AHCI_CAP2_SDS		(1 << 3)
#define  AHCI_CAP2_APST		(1 << 2)
#define  AHCI_CAP2_NVMP		(1 << 1)
#define  AHCI_CAP2_BOH		(1 << 0)
#define AHCI_REG_BOHC		0x28		/* BIOS/OS Handoff Control and Status */
#define  AHCI_BOHC_BB		(1 << 4)
#define  AHCI_BOHC_OOC		(1 << 3)
#define  AHCI_BOHC_SOOE		(1 << 2)
#define  AHCI_BOHC_OOS		(1 << 0)
#define  AHCI_BOHC_BOS		(1 << 0)
#define AHCI_REG_PxCLB(x)	(0x100 + (x) * 0x80)
#define AHCI_REG_PxCLBU(x)	(AHCI_REG_PxCLB(x)+0x04)
#define AHCI_REG_PxFB(x)	(AHCI_REG_PxCLB(x)+0x08)
#define AHCI_REG_PxFBU(x)	(AHCI_REG_PxCLB(x)+0x0c)
#define AHCI_REG_PxIS(x)	(AHCI_REG_PxCLB(x)+0x10)
#define  AHCI_PxIS_CPDS(x)	(((x) >> 31) & 1)
#define  AHCI_PxIS_TFES(x)	(((x) >> 30) & 1)
#define  AHCI_PxIS_HBFS(x)	(((x) >> 29) & 1)
#define  AHCI_PxIS_HBDS(x)	(((x) >> 28) & 1)
#define  AHCI_PxIS_IFS(x)	(((x) >> 27) & 1)
#define  AHCI_PxIS_INFS(x)	(((x) >> 26) & 1)
#define  AHCI_PxIS_OFS(x)	(((x) >> 24) & 1)
#define  AHCI_PxIS_IPMS(x)	(((x) >> 23) & 1)
#define  AHCI_PxIS_PRCS(x)	(((x) >> 22) & 1)
#define  AHCI_PxIS_DMPS(x)	(((x) >> 7) & 1)
#define  AHCI_PxIS_PCS(x)	(((x) >> 6) & 1)
#define  AHCI_PxIS_DPS(x)	(((x) >> 5) & 1)
#define  AHCI_PxIS_UFS(x)	(((x) >> 4) & 1)
#define  AHCI_PxIS_SDBS(x)	(((x) >> 3) & 1)
#define  AHCI_PxIS_DSS(x)	(((x) >> 2) & 1)
#define  AHCI_PxIS_PSS(x)	(((x) >> 1) & 1)
#define  AHCI_PxIS_DHRS(x)	((x) & 1)
#define AHCI_REG_PxIE(x)	(AHCI_REG_PxCLB(x)+0x14)
#define  AHCI_PxIE_CPDE		(1 << 31)
#define  AHCI_PxIE_TFEE		(1 << 30)
#define  AHCI_PxIE_HBFE		(1 << 29)
#define  AHCI_PxIE_HBDE		(1 << 28)
#define  AHCI_PxIE_IFE		(1 << 27)
#define  AHCI_PxIE_INFE		(1 << 26)
#define  AHCI_PxIE_OFE		(1 << 24)
#define  AHCI_PxIE_IPME		(1 << 23)
#define  AHCI_PxIE_PRCE		(1 << 22)
#define  AHCI_PxIE_DMPE		(1 << 7)
#define  AHCI_PxIE_PCE		(1 << 6)
#define  AHCI_PxIE_DPE		(1 << 5)
#define  AHCI_PxIE_UFE		(1 << 4)
#define  AHCI_PxIE_SDBE		(1 << 3)
#define  AHCI_PxIE_DSE		(1 << 2)
#define  AHCI_PxIE_PSE		(1 << 1)
#define  AHCI_PxIE_DHRE		(1 << 0)
#define AHCI_REG_PxCMD(x)	(AHCI_REG_PxCLB(x)+0x18)
#define  AHCI_PxCMD_ICC(x)	((x) << 28)
#define   AHCI_ICC_DEVSLEEP	8
#define   AHCI_ICC_SLUMBER	6
#define   AHCI_ICC_PARTIAL	2
#define   AHCI_ICC_ACTIVE	1
#define   AHCI_ICC_IDLE		0
#define  AHCI_PxCMD_ASP		(1 << 27)
#define  AHCI_PxCMD_ALPE	(1 << 26)
#define  AHCI_PxCMD_DLAE	(1 << 25)
#define  AHCI_PxCMD_ATAPI	(1 << 24)
#define  AHCI_PxCMD_APSTE	(1 << 23)
#define  AHCI_PxCMD_FBSCP	(1 << 22)
#define  AHCI_PxCMD_ESP		(1 << 21)
#define  AHCI_PxCMD_CPD		(1 << 20)
#define  AHCI_PxCMD_MPSP	(1 << 19)
#define  AHCI_PxCMD_HPCP	(1 << 18)
#define  AHCI_PxCMD_PMA		(1 << 17)
#define  AHCI_PxCMD_CPS		(1 << 16)
#define  AHCI_PxCMD_CR		(1 << 15)
#define  AHCI_PxCMD_FR		(1 << 14)
#define  AHCI_PxCMD_MPSS(x)	((x) << 13)
#define  AHCI_PxCMD_CCS(x)	(((x) >> 8) & 0x1f)
#define  AHCI_PxCMD_FRE		(1 << 4)
#define  AHCI_PxCMD_CLO		(1 << 3)
#define  AHCI_PxCMD_POD		(1 << 2)
#define  AHCI_PxCMD_SUD		(1 << 1)
#define  AHCI_PxCMD_ST		(1 << 0)
#define AHCI_REG_PxTFD(x)	(AHCI_REG_PxCLB(x)+0x20)
#define  AHCI_PxTFD_ERR(x)	(((x) >> 8) & 0xff)
#define  AHCI_PxTFD_STS(x)	((x) & 0xff)
#define  AHCI_PxTFD_STS_BSY	(1 << 7)
#define  AHCI_PxTFD_STS_DRQ	(1 << 3)
#define  AHCI_PxTFD_STS_ERR	(1 << 0)
#define AHCI_REG_PxSIG(x)	(AHCI_REG_PxCLB(x)+0x24)
#define AHCI_REG_PxSSTS(x)	(AHCI_REG_PxCLB(x)+0x28)
#define  AHCI_PxSSTS_IPM(x)	(((x) >> 8) & 0xf)
#define  AHCI_PxSSTS_SPD(x)	(((x) >> 4) & 0xf)
#define  AHCI_PxSSTS_DETD(x)	((x) & 0xf)
#define   AHCI_DETD_NO_DEV_NO_PHY	0
#define   AHCI_DETD_DEV_NO_PHY		1
#define   AHCI_DETD_DEV_PHY		3
#define   AHCI_DETD_OFFLINE		4
#define AHCI_REG_PxSCTL(x)	(AHCI_REG_PxCLB(x)+0x2c)
#define  AHCI_PxSCTL_IPM(x)	((x) << 8)
#define  AHCI_PxSCTL_SPD(x)	((x) << 4)
#define  AHCI_PxSCTL_DET(x)	(x)
#define   AHCI_DET_NONE			0
#define   AHCI_DET_RESET		1
#define   AHCI_DET_DISABLE		4
#define AHCI_REG_PxSERR(x)	(AHCI_REG_PxCLB(x)+0x30)
#define  AHCI_PxSERR_DIAG_X(x)	(((x) >> 26) & 1)
#define  AHCI_PxSERR_DIAG_F(x)	(((x) >> 25) & 1)
#define  AHCI_PxSERR_DIAG_T(x)	(((x) >> 24) & 1)
#define  AHCI_PxSERR_DIAG_S(x)	(((x) >> 23) & 1)
#define  AHCI_PxSERR_DIAG_H(x)	(((x) >> 22) & 1)
#define  AHCI_PxSERR_DIAG_C(x)	(((x) >> 21) & 1)
#define  AHCI_PxSERR_DIAG_B(x)	(((x) >> 19) & 1)
#define  AHCI_PxSERR_DIAG_W(x)	(((x) >> 18) & 1)
#define  AHCI_PxSERR_DIAG_I(x)	(((x) >> 17) & 1)
#define  AHCI_PxSERR_DIAG_N(x)	(((x) >> 16) & 1)
#define  AHCI_PxSERR_ERR_E(x)	(((x) >> 11) & 1)
#define  AHCI_PxSERR_ERR_P(x)	(((x) >> 10) & 1)
#define  AHCI_PxSERR_ERR_C(x)	(((x) >> 9) & 1)
#define  AHCI_PxSERR_ERR_T(x)	(((x) >> 8) & 1)
#define  AHCI_PxSERR_ERR_M(x)	(((x) >> 1) & 1)
#define  AHCI_PxSERR_ERR_I(x)	((x) & 1)
#define AHCI_REG_PxSACT(x)	(AHCI_REG_PxCLB(x)+0x34)
#define  AHCI_PxSACT_DS(x)	(1 << (x))
#define AHCI_REG_PxCI(x)	(AHCI_REG_PxCLB(x)+0x38)
#define  AHCI_PxCIT_CI(x)	(1 << (x))
#define AHCI_REG_PxSNTF(x)	(AHCI_REG_PxCLB(x)+0x3c)
#define  AHCI_PxSNTF_PMN(x)	(1 << (x))
#define AHCI_REG_PxFBS(x)	(AHCI_REG_PxCLB(x)+0x40)
#define  AHCI_PxFBS_DWE(x)	(((x) >> 16) & 0xf)
#define  AHCI_PxFBS_ADO(x)	(((x) >> 12) & 0xf)
#define  AHCI_PxFBS_DEV(x)	((x) << 8)
#define  AHCI_PxFBS_SDE(x)	(((x) >> 2) & 1)
#define  AHCI_PxFBS_DEC		(1 << 1)
#define  AHCI_PxFBS_EN		(1 << 0)
#define AHCI_REG_PxDEVSLP(x)	(AHCI_REG_PxCLB(x)+0x44)
#define  AHCI_PxDEVSLP_DM(x)	(((x) >> 25) & 0xf)
#define  AHCI_PxDEVSLP_DITO(x)	((x) << 15)
#define  AHCI_PxDEVSLP_MDAT(x)	((x) << 10)
#define  AHCI_PxDEVSLP_DETO(x)	((x) << 2)
#define  AHCI_PxDEVSLP_DSP(x)	(((x) >> 1) & 1)
#define  AHCI_PxDEVSLP_ADSE(x)	(1 << 0)

/* Received FIS structure, 4.2.1 */
struct AHCI_PCI_RFIS {
	union {
		/*struct AHCI_PCI_DSFIS	ds;*/
		uint8_t			data[0x20];
	} rfis_ds;
	union {
		/*struct AHCI_PCI_PSFIS	ps;*/
		uint8_t			data[0x20];
	} rfis_ps;
	union {
		/*struct AHCI_PCI_D2HFIS	d2h;*/
		uint8_t			data[0x18];
	} rfis_d2h;
	union {
		/*struct AHCI_PCI_SDBFIS	sdb;*/
		uint8_t			data[0x8];
	} rfis_sdb;
	union {
		uint8_t			data[0x40];
	} rfis_u;
	union {
		uint8_t			data[0x5f];
	} rfis_rsvd;
} __attribute__((packed));

/* Command list structure entry */
struct AHCI_PCI_CLE {
	uint32_t	cle_dw0;
#define AHCI_CLE_DW0_PRDTL(x)		((x) << 16)
#define AHCI_CLE_DW0_PMP(x)		((x) << 12)
#define AHCI_CLE_DW0_C			(1 << 10)
#define AHCI_CLE_DW0_B			(1 << 9)
#define AHCI_CLE_DW0_R			(1 << 8)
#define AHCI_CLE_DW0_P			(1 << 7)
#define AHCI_CLE_DW0_W			(1 << 6)
#define AHCI_CLE_DW0_A			(1 << 4)
#define AHCI_CLE_DW0_CFL(x)		(x)
	uint32_t	cle_dw1;
#define AHCI_CLE_DW1_PRDBC(x)		(x)
	uint32_t	cle_dw2;
#define AHCI_CLE_DW2_CTBA(x)		(x)
	uint32_t	cle_dw3;
#define AHCI_CLE_DW3_CTBAU(x)		(x)
	uint32_t	cle_dw4;
	uint32_t	cle_dw5;
	uint32_t	cle_dw6;
	uint32_t	cle_dw7;
} __attribute__((packed));

/* Physical Region Descriptor Entry */
struct AHCI_PCI_PRDE {
	uint32_t	prde_dw0;
#define AHCI_PRDE_DW0_DBA(x)		(x)
	uint32_t	prde_dw1;
#define AHCI_PRDE_DW1_DBAU(x)		(x)
	uint32_t	prde_dw2;
	uint32_t	prde_dw3;
#define AHCI_PRDE_DW3_I			(1 << 31)
#define AHCI_PRDE_DW3_DBC(x)		(x)
} __attribute__((packed));

/* Command Table */
struct AHCI_PCI_CT {
	uint8_t		ct_cfis[64];
	uint8_t		ct_acmd[16];
	uint8_t		ct_rsvd[48];
	/*
	 * Note that even though AHCI supports multiple PRD's, we only use a
	 * single one.
	 */
	struct		AHCI_PCI_PRDE ct_prd[1];
} __attribute__((packed));

#if AHCI_DEBUG
#define DUMP_PORT_STATE(n) \
		kprintf("[%d] port #%d -> tfd %x ssts %x serr %x\n", __LINE__, n, AHCI_READ_4(AHCI_REG_PxTFD(n)), AHCI_READ_4(AHCI_REG_PxSSTS(n)), \
		 AHCI_READ_4(AHCI_REG_PxSERR(n)))
#else
#define DUMP_PORT_STATE(n)
#endif

} // namespace AHCI
} // namespace Ananas

#endif /* __ANANAS_AHCIPCI_H__ */
