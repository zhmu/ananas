/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __ACPI_RESOURCE_H__
#define __ACPI_RESOURCE_H__

class ResourceSet;

ACPI_STATUS acpi_process_resources(ACPI_HANDLE ObjHandle, ResourceSet& resourceSet);

#endif /* __ACPI_RESOURCE_H__ */
