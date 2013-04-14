#ifndef __ANANAS_X86_PCIHB_H__
#define __ANANAS_X86_PCIHB_H__

#include <ananas/types.h>

#define PCI_CFG1_ADDR 0xcf8			/* PCI Configuration Mechanism 1 - address port */
#define  PCI_CFG1_ADDR_ENABLE (1 << 31)
#define PCI_CFG1_DATA 0xcfc			/* PCI Configuration Mechanism 1 - data port */

#define PCI_MAKE_ADDR(bus, dev, func, reg) \
	(PCI_CFG1_ADDR_ENABLE | ((bus) << 16) | ((dev) << 11) | ((func) << 8) | (reg))

#endif /* __ANANAS_X86_PCIHB_H__ */
