#ifndef __ANANAS_X86_ACPI_H__
#define __ANANAS_X86_ACPI_H__

#include <ananas/types.h>

void acpi_init();
errorcode_t acpi_smp_init(int* bsp_apic_id);

#endif /* __ANANAS_X86_ACPI_H__ */
