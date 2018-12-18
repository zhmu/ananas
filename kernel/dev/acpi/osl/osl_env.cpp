/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "../acpica/acpi.h"

ACPI_STATUS
AcpiOsInitialize() { return AE_OK; }

ACPI_STATUS
AcpiOsTerminate() { return AE_OK; }

ACPI_PHYSICAL_ADDRESS
AcpiOsGetRootPointer()
{
    ACPI_PHYSICAL_ADDRESS addr;
    if (AcpiFindRootPointer(&addr) == AE_OK)
        return addr;
    return (ACPI_PHYSICAL_ADDRESS)NULL;
}

ACPI_STATUS
AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES* PredefinedObject, ACPI_STRING* NewValue)
{
    *NewValue = NULL;
    return AE_OK;
}

ACPI_STATUS
AcpiOsTableOverride(ACPI_TABLE_HEADER* ExistingTable, ACPI_TABLE_HEADER** NewTable)
{
    *NewTable = NULL;
    return AE_OK;
}

ACPI_STATUS
AcpiOsPhysicalTableOverride(
    ACPI_TABLE_HEADER* ExistingTable, ACPI_PHYSICAL_ADDRESS* NewAddress, UINT32* NewTableLength)
{
    *NewAddress = (ACPI_PHYSICAL_ADDRESS)NULL;
    return AE_OK;
}
