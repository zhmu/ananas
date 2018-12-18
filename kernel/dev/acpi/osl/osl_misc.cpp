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

/* vim:set ts=2 sw=2: */
