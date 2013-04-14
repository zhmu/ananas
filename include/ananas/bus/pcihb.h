#ifndef __ANANAS_BUS_PCIHB_H__
#define __ANANAS_BUS_PCIHB_H__

#include <ananas/types.h>

uint32_t pci_read_config(uint32_t bus, uint32_t dev, uint32_t func, uint32_t reg, int width);
void pci_write_config(uint32_t bus, uint32_t dev, uint32_t func, uint32_t reg, uint32_t value, int width);

#endif /* __ANANAS_BUS_PCIHB_H__ */
