#ifndef __BUS_PCI_H__
#define __BUS_PCI_H__

#include <ananas/types.h>

namespace Ananas {
class Device;
}

#define PCI_NOVENDOR		0xFFFF
#define PCI_MAX_BUSSES		256
#define PCI_MAX_DEVICES		32
#define PCI_MAX_FUNCS		8

/* PCI configuration registers */
#define PCI_REG_DEVICEVENDOR	0x00
#define PCI_REG_STATUSCOMMAND	0x04
#define  PCI_STAT_DPE		0x80000000 /* Detect Parity Error */
#define  PCI_STAT_SSE		0x40000000 /* Signalled System Error */
#define  PCI_STAT_RMA		0x20000000 /* Received Master-Abort */
#define  PCI_STAT_RTA		0x10000000 /* Received Target-Abort */
#define  PCI_STAT_STA		0x08000000 /* Signalled Target-Abort */
#define  PCI_STAT_DT		0x04000000 /* DEVSEL Timing */
#define  PCI_STAT_DPR		0x01000000 /* Data Parity Reported */
#define  PCI_STAT_FBBC		0x00800000 /* Fast Back-to-back capable */
#define  PCI_STAT_UDF		0x00400000 /* User Defined Features supported */
#define  PCI_STAT_66		0x00200000 /* 66MHz Capable */
#define  PCI_CMD_BBE		0x00000200 /* Fast Back-to-back enable */
#define  PCI_CMD_SSE		0x00000100 /* System Error Enable */
#define  PCI_CMD_WC		0x00000080 /* Wait Cycle Enable */
#define  PCI_CMD_PER		0x00000040 /* Parity Error Response */
#define  PCI_CMD_VPS		0x00000020 /* VGA Palette Snoop Enable */
#define  PCI_CMD_MWI		0x00000010 /* Memory Write and Invalidate */
#define  PCI_CMD_SC		0x00000008 /* Special Cycle Recognition */
#define  PCI_CMD_BM		0x00000004 /* Bus Master Enable */
#define  PCI_CMD_MA		0x00000002 /* Memory Access Enable */
#define  PCI_CMD_IO		0x00000001 /* I/O Access Enable */
#define PCI_REG_CLASSREVISION	0x08
#define PCI_CLASS(x)		((x) >> 24)
#define PCI_SUBCLASS(x)		(((x) >> 16) & 0xff)
#define PCI_PROGINT(x)		(((x) >> 8) & 0xff)
#define PCI_REVISION(x)		((x) & 0xff)
#define  PCI_CLASS_STORAGE	0x01       /* Mass storage controller */
#define   PCI_SUBCLASS_SCSI	0x00
#define   PCI_SUBCLASS_IDE	0x01
#define   PCI_SUBCLASS_FLOPPY	0x02
#define   PCI_SUBCLASS_IPI	0x03
#define   PCI_SUBCLASS_RAID	0x04
#define   PCI_SUBCLASS_SATA	0x06
#define  PCI_CLASS_NETWORK	0x02       /* Network controller */
#define   PCI_SUBCLASS_ETHERNET	0x00
#define   PCI_SUBCLASS_TOKEN	0x01
#define   PCI_SUBCLASS_FDDI	0x02
#define   PCI_SUBCLASS_ATM	0x03
#define  PCI_CLASS_DISPLAY	0x03       /* Display controller */
#define   PCI_SUBCLASS_VGA	0x00
#define  PCI_CLASS_MULTIMEDIA	0x04       /* Multimedia controller */
#define   PCI_SUBCLASS_VIDEO	0x00
#define   PCI_SUBCLASS_AUDIO	0x01
#define  PCI_CLASS_MEMORY	0x05       /* Memory controller */
#define   PCI_SUBCLASS_RAM	0x00
#define   PCI_SUBCLASS_FLASH	0x01
#define  PCI_CLASS_BRIDGE	0x06       /* Bridge device */
#define   PCI_SUBCLASS_HOST	0x00
#define   PCI_SUBCLASS_PCIISA	0x01
#define   PCI_SUBCLASS_PCIEISA	0x02
#define   PCI_SUBCLASS_PCIMCA	0x03
#define   PCI_SUBCLASS_PCIPCI	0x04
#define   PCI_SUBCLASS_PCIPCMCIA 0x05
#define   PCI_SUBCLASS_PCINUBUS	0x06
#define   PCI_SUBCLASS_PCICARDBUS 0x07
#define  PCI_CLASS_COMM		0x07       /* Simple comm controller */
#define   PCI_SUBCLASS_GENERICSERIAL	0x00
#define   PCI_SUBCLASS_PARALLEL	0x01
#define  PCI_CLASS_BASE		0x08       /* Base system peripherals */
#define   PCI_SUBCLASS_PIC	0x00
#define   PCI_SUBCLASS_DMA	0x01
#define   PCI_SUBCLASS_TIMER	0x02
#define   PCI_SUBCLASS_RTC	0x03
#define  PCI_CLASS_INPUT	0x09       /* Input devices */
#define   PCI_SUBCLASS_KEYBOARD	0x00
#define   PCI_SUBCLASS_PEN	0x01
#define   PCI_SUBCLASS_MOUSE	0x02
#define  PCI_CLASS_DOCK		0x0a       /* Docking stations */
#define   PCI_SUBCLASS_GENERIC	0x00
#define  PCI_CLASS_PROCESSOR	0x0b       /* Processor */
#define   PCI_SUBCLASS_386	0x00
#define   PCI_SUBCLASS_486	0x01
#define   PCI_SUBCLASS_PENTIUM	0x02
#define   PCI_SUBCLASS_ALPHA	0x03
#define   PCI_SUBCLASS_POWERPC	0x04
#define   PCI_SUBCLASS_COPROC	0x40
#define  PCI_CLASS_SERIAL	0x0c       /* Serial bus controller */
#define   PCI_SUBCLASS_FIREWIRE	0x00
#define   PCI_SUBCLASS_ACCESS	0x01
#define   PCI_SUBCLASS_SSA	0x02
#define   PCI_SUBCLASS_USB	0x03
#define  PCI_CLASS_MISC		0xff       /* Misc */
#define PCI_REG_HEADERTIMER	0x0c
#define  PCI_HEADER_MULTI	0x800000
#define PCI_REG_BAR0		0x10
#define  PCI_BAR_MMIO		0x01
#define PCI_REG_BAR1		0x14
#define PCI_REG_BAR2		0x18
#define PCI_REG_BAR3		0x1c
#define PCI_REG_BAR4		0x20
#define PCI_REG_BAR5		0x24
#define PCI_REG_CIS		0x28
#define PCI_REG_SUBSYSTEMID	0x2c
#define PCI_REG_EXPANSIONROM	0x30
#define PCI_REG_CAPABILITIES	0x34
#define _PCI_REG_RESERVED	0x38
#define PCI_REG_INTERRUPT	0x3c

void pci_write_cfg(Ananas::Device& dev, uint32_t reg, uint32_t val, int size);
uint32_t pci_read_cfg(Ananas::Device& dev, uint32_t reg, int size);
void pci_enable_busmaster(Ananas::Device& dev, bool on);

#endif /* __BUS_PCI_H__ */
