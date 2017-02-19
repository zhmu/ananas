#ifndef __ACPI_RESOURCE_H__
#define __ACPI_RESOURCE_H__

namespace Ananas {
class ResourceSet;
}

ACPI_STATUS acpi_process_resources(ACPI_HANDLE ObjHandle, Ananas::ResourceSet& resourceSet);

#endif /* __ACPI_RESOURCE_H__ */
