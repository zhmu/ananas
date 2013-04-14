#ifndef __ACPI_RESOURCE_H__
#define __ACPI_RESOURCE_H__

/* Convert a PNPxxxx to the resource-represenation - assumes the PNP prefix is in place */
#define ACPI_PNP_ID(s) \
	((s)[4] << 24 | (s)[5] << 16 | (s)[6] << 8 | (s)[7])

ACPI_STATUS acpi_process_resources(ACPI_HANDLE ObjHandle, device_t dev);

#endif /* __ACPI_RESOURCE_H__ */
