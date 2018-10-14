#ifndef __ACPI_RESOURCE_H__
#define __ACPI_RESOURCE_H__

class ResourceSet;

ACPI_STATUS acpi_process_resources(ACPI_HANDLE ObjHandle, ResourceSet& resourceSet);

#endif /* __ACPI_RESOURCE_H__ */
