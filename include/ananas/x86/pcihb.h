#ifndef __ANANAS_X86_PCIHB_H__
#define __ANANAS_X86_PCIHB_H__

#include <ananas/types.h>
#include <ananas/x86/io.h>

/* We use PCI configuration mechanism 1 as that is most commonly supported */
#define PCI_CFG1_ADDR 0xcf8			/* PCI Configuration Mechanism 1 - address port */
#define  PCI_CFG1_ADDR_ENABLE (1 << 31)
#define PCI_CFG1_DATA 0xcfc			/* PCI Configuration Mechanism 1 - data port */

static inline unsigned int
pci_make_addr(uint32_t bus, uint32_t dev, uint32_t func, uint32_t reg)
{
	return PCI_CFG1_ADDR_ENABLE | (bus << 16) | (dev << 11) | (func << 8) | reg;
}

static inline uint32_t
pci_read_config(uint32_t bus, uint32_t dev, uint32_t func, uint32_t reg, int width)
{
	outl(PCI_CFG1_ADDR, pci_make_addr(bus, dev, func, reg));
	switch(width) {
		case 32: return inl(PCI_CFG1_DATA);
		case 16: return inw(PCI_CFG1_DATA);
		case  8: return inb(PCI_CFG1_DATA);
		default: panic("unsupported width %u", width);
	}
}

static void
pci_write_config(uint32_t bus, uint32_t dev, uint32_t func, uint32_t reg, uint32_t value, int width)
{
	outl(PCI_CFG1_ADDR, pci_make_addr(bus, dev, func, reg));
	switch(width) {
		case 32: outb(PCI_CFG1_DATA, value); break;
		case 16: outw(PCI_CFG1_DATA, value); break;
		case  8: outl(PCI_CFG1_DATA, value); break;
		default: panic("unsupported width %u", width);
	}
}

/* vim:set ts=2 sw=2: */

#endif /* __ANANAS_X86_PCIHB_H__ */
