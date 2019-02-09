/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/lib.h"
#include "kernel/lock.h"
#include "kernel/mm.h"
#include "../acpica/acpi.h"

ACPI_STATUS
AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_SEMAPHORE* OutHandle)
{
    auto sem = new Semaphore("acpi", MaxUnits);
    if (sem == NULL)
        return AE_NO_MEMORY;

    while (InitialUnits > MaxUnits) {
        sem->Wait();
        InitialUnits--;
    }
    *OutHandle = sem;
    return AE_OK;
}

ACPI_STATUS
AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle)
{
    if (Handle == NULL)
        return AE_BAD_PARAMETER;
    delete Handle;
    return AE_OK;
}

ACPI_STATUS
AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout)
{
    KASSERT(Units == 1, "unsupported number of units");
    /*KASSERT(Timeout == -1, "unsupported timout value");*/
    if (Handle == NULL)
        return AE_BAD_PARAMETER;
    Handle->Wait();
    return AE_OK;
}

ACPI_STATUS
AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units)
{
    KASSERT(Units == 1, "unsupported number of units");
    if (Handle == NULL)
        return AE_BAD_PARAMETER;
    Handle->Signal();
    return AE_OK;
}

ACPI_STATUS
AcpiOsCreateLock(ACPI_SPINLOCK* OutHandle)
{
    *OutHandle = new Spinlock;
    return *OutHandle != nullptr ? AE_OK : AE_NO_MEMORY;
}

void AcpiOsDeleteLock(ACPI_SPINLOCK Handle) { delete Handle; }

ACPI_CPU_FLAGS
AcpiOsAcquireLock(ACPI_SPINLOCK Handle) { return Handle->LockUnpremptible(); }

void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags)
{
    Handle->UnlockUnpremptible(Flags);
}
