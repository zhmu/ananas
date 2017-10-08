#include "acpica/acpi.h"
#include "acpi.h"
#include "acpi_resource.h"
#include "kernel/device.h"

using Resource = Ananas::Resource;

ACPI_STATUS
acpi_process_resources(ACPI_HANDLE ObjHandle, Ananas::ResourceSet& resourceSet)
{
	/* Fetch device information; we need the hardware ID */
	ACPI_DEVICE_INFO* Info;
	if (ACPI_FAILURE(AcpiGetObjectInfo (ObjHandle, &Info)))
		return AE_OK;

	/* If we have a PNP hardware ID, store it */
	if (Info->HardwareId.String != NULL && Info->HardwareId.Length >= ACPI_EISAID_STRING_SIZE &&
	    strncmp(Info->HardwareId.String, "PNP", 3) == 0) {
		/* Fetch the value of the PNPxxxx string */
		unsigned int pnpid = (unsigned int)strtoul(Info->HardwareId.String + 3 /* skip PNP */, NULL, 16);
		resourceSet.AddResource(Resource(Resource::RT_PNP_ID, pnpid, 0));
		ACPI_DPRINTF("> [%s]\n", Info->HardwareId.String);
	}
 	AcpiOsFree(Info);

	/* Fetch the ACPI resources */
	ACPI_BUFFER buf;
	buf.Length = ACPI_ALLOCATE_BUFFER;
	ACPI_STATUS status = AcpiGetCurrentResources(ObjHandle, &buf);
	if (ACPI_FAILURE(status))
		return status;

	/* Now, process the resources */
	char* end = (char*)buf.Pointer + buf.Length;
	for (char* ptr = static_cast<char*>(buf.Pointer); ptr < end; /* nothing */) {
		ACPI_RESOURCE* res = (ACPI_RESOURCE*)ptr;
		ptr += res->Length;

		ACPI_DPRINTF("type %u\n", res->Type);
		switch(res->Type) {
			case ACPI_RESOURCE_TYPE_END_TAG:
				ptr = end;
				break;
			case ACPI_RESOURCE_TYPE_IRQ:
				ACPI_DPRINTF("irq:");
				for (int i = 0; i < res->Data.Irq.InterruptCount; i++) {
					ACPI_DPRINTF(" %x", res->Data.Irq.Interrupts[i]);
					resourceSet.AddResource(Resource(Resource::RT_IRQ, res->Data.Irq.Interrupts[i], 0));
				}
				ACPI_DPRINTF("\n");
				break;
			case ACPI_RESOURCE_TYPE_IO:
				ACPI_DPRINTF("i/o: %x/%d\n", res->Data.Io.Minimum, res->Data.Io.AddressLength);
				resourceSet.AddResource(Resource(Resource::RT_IO, res->Data.Io.Minimum, res->Data.Io.AddressLength));
				break;
			case ACPI_RESOURCE_TYPE_ADDRESS16:
				ACPI_DPRINTF("addr16: %x/%d\n", res->Data.Address16.Minimum, res->Data.Address16.AddressLength);
				resourceSet.AddResource(Resource(Resource::RT_Memory, res->Data.Address16.Minimum, res->Data.Address16.AddressLength));
				break;
			case ACPI_RESOURCE_TYPE_ADDRESS32:
				ACPI_DPRINTF("addr32: %x/%d\n", res->Data.Address32.Minimum, res->Data.Address32.AddressLength);
				resourceSet.AddResource(Resource(Resource::RT_Memory,res->Data.Address32.Minimum, res->Data.Address32.AddressLength));
				break;
		}
	}
	
	AcpiOsFree(buf.Pointer);
	return AE_OK;
}

/* vim:set ts=2 sw=2: */
