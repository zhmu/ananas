/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __ANANAS_X86_ACPI_H__
#define __ANANAS_X86_ACPI_H__

class Result;

void acpi_init();
Result acpi_smp_init(int* bsp_apic_id);

#endif /* __ANANAS_X86_ACPI_H__ */
