/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/lib.h"
#include "../acpica/acpi.h"

ACPI_STATUS
AcpiOsSignal(UINT32 Function, void* Info)
{
    panic("AcpiOsSignal");
    return AE_OK;
}

void ACPI_INTERNAL_VAR_XFACE AcpiOsPrintf(const char* Format, ...)
{
    va_list va;
    va_start(va, Format);
    AcpiOsVprintf(Format, va);
    va_end(va);
}

void AcpiOsVprintf(const char* Format, va_list Args) { vaprintf(Format, Args); }

UINT64
AcpiOsGetTimer()
{
    panic("AcpiOsGetTimer");
    return 0;
}
