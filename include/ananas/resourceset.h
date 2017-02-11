#ifndef __ANANAS_RESOURCE_H__
#define __ANANAS_RESOURCE_H__

#include <ananas/types.h>

/* Resources types */
enum RESOURCE_TYPE {
	RESTYPE_UNUSED,
	/* Base resource types */
	RESTYPE_MEMORY,
	RESTYPE_IO,
	RESTYPE_IRQ,
	/* Generic child devices */
	RESTYPE_CHILDNUM,
	/* PCI-specific resource types */
	RESTYPE_PCI_VENDORID,
	RESTYPE_PCI_DEVICEID,
	RESTYPE_PCI_BUS,
	RESTYPE_PCI_DEVICE,
	RESTYPE_PCI_FUNCTION,
	RESTYPE_PCI_CLASSREV,
	/* PnP ID - used by ACPI */
	RESTYPE_PNP_ID,
	/* USB-specific */
	RESTYPE_USB_DEVICE,
};

typedef enum RESOURCE_TYPE resource_type_t;
typedef uintptr_t resource_base_t;
typedef size_t resource_length_t;

/* A resource is just a type with an <base,length> tuple */
struct RESOURCE {
	resource_type_t r_type;
	resource_base_t r_base;
	resource_length_t r_length;
};
typedef struct RESOURCE resource_t;

/* Maximum number of resources a given resource set can have */
#define RESOURCESET_MAX_ENTRIES 16

struct RESOURCE_SET {
	resource_t rs_resource[RESOURCESET_MAX_ENTRIES];
};

typedef struct RESOURCE_SET resource_set_t;

void* resourceset_alloc_resource(resource_set_t* resources, resource_type_t type, resource_length_t len);
int resourceset_add_resource(resource_set_t* resourceset, resource_type_t type, resource_base_t base, resource_length_t len);
resource_t* resourceset_get_resource(resource_set_t* resources, resource_type_t type, int index);
void resourceset_copy(resource_set_t* dst, const resource_set_t* src);
void resourceset_print(resource_set_t* rs);

#endif /* __ANANAS_RESOURCE_H__ */
