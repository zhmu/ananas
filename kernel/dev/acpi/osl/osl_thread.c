#include <ananas/pcpu.h>
#include <ananas/time.h>
#include "../acpica/acpi.h"

ACPI_THREAD_ID
AcpiOsGetThreadId()
{
	return (ACPI_THREAD_ID)(uintptr_t)PCPU_GET(curthread);
}

ACPI_STATUS
AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void* Context)
{
	panic("AcpiOsExecute()");
	return AE_BAD_PARAMETER;
}

void
AcpiOsSleep(UINT64 Milliseconds)
{
	AcpiOsStall(Milliseconds); // for now
}

void
AcpiOsStall(UINT32 Milliseconds)
{
	delay(Milliseconds); // for now
}

void
AcpiOsWaitEventsComplete()
{
	panic("AcpiOsWaitEventsComplete()");
}

/* vim:set ts=2 sw=2: */
