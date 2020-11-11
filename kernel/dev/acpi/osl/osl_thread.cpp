/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/lib.h"
#include "kernel/time.h"
#include "kernel/pcpu.h"
#include "kernel/thread.h"
#include "../acpica/acpi.h"

ACPI_THREAD_ID
AcpiOsGetThreadId() { return (ACPI_THREAD_ID)(uintptr_t)&thread::GetCurrent(); }

ACPI_STATUS
AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void* Context)
{
    panic("AcpiOsExecute()");
    return AE_BAD_PARAMETER;
}

void AcpiOsSleep(UINT64 Milliseconds)
{
    AcpiOsStall(Milliseconds); // for now
}

void AcpiOsStall(UINT32 Milliseconds)
{
    delay(Milliseconds); // for now
}

void AcpiOsWaitEventsComplete() { panic("AcpiOsWaitEventsComplete()"); }
