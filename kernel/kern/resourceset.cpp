#include <ananas/resourceset.h>
#include <ananas/kmem.h>
#include <ananas/vm.h>
#include <ananas/lib.h>

void
resourceset_copy(resource_set_t* dst, const resource_set_t* src)
{
	memcpy(&dst->rs_resource, &src->rs_resource, sizeof(resource_t) * RESOURCESET_MAX_ENTRIES);
}

struct RESOURCE*
resourceset_get_resource(resource_set_t* rs, resource_type_t type, int index)
{
	resource_t* res = &rs->rs_resource[0];
	for (int i = 0; i < RESOURCESET_MAX_ENTRIES; i++, res++) {
		if (res->r_type != type)
			continue;
		if (index-- > 0)
			continue;
		return res;
	}

	return NULL;
}

void*
resourceset_alloc_resource(resource_set_t* rs, resource_type_t type, size_t len)
{
	resource_t* res = resourceset_get_resource(rs, type, 0);
	if (res == NULL)
		/* No such resource! */
		return NULL;

	res->r_length = len;

	switch(type) {
		case RESTYPE_MEMORY:
			return (void*)kmem_map(res->r_base, len, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_DEVICE);
		case RESTYPE_IO:
		case RESTYPE_CHILDNUM:
		case RESTYPE_IRQ: /* XXX should allocate, not just return */
		case RESTYPE_USB_DEVICE:
			return (void*)res->r_base;
		default:
			panic("resource type %u exists, but can't allocate", type);
	}

	/* NOTREACHED */
	return NULL;
}

int
resourceset_add_resource(resource_set_t* rs, resource_type_t type, resource_base_t base, resource_length_t len)
{
	resource_t* res = &rs->rs_resource[0];
	for (int i = 0; i < RESOURCESET_MAX_ENTRIES; i++, res++) {
		if (res->r_type != RESTYPE_UNUSED)
			continue;
		res->r_type = type;
		res->r_base = base;
		res->r_length = len;
		return 1;
	}

	return 0;
}

void
resourceset_print(resource_set_t* rs)
{
	resource_t* res = &rs->rs_resource[0];
	for (int i = 0; i < RESOURCESET_MAX_ENTRIES; i++, res++) {
		int hex = 0;
		switch (res->r_type) {
			case RESTYPE_MEMORY: kprintf("memory "); hex = 1; break;
			case RESTYPE_IO: kprintf("io "); hex = 1; break;
			case RESTYPE_IRQ: kprintf("irq "); break;
			case RESTYPE_CHILDNUM: kprintf("child "); break;
			case RESTYPE_PCI_BUS: kprintf("bus "); break;
			case RESTYPE_PCI_DEVICE: kprintf("dev "); break;
			case RESTYPE_PCI_FUNCTION: kprintf("func "); break;
			case RESTYPE_PCI_VENDORID: kprintf("vendor "); hex = 1; break;
			case RESTYPE_PCI_DEVICEID: kprintf("device "); hex = 1; break;
			case RESTYPE_PCI_CLASSREV: kprintf("class/revision "); break;
			default: continue;
		}
		kprintf(hex ? "0x%x" : "%u", res->r_base);
		if (res->r_length > 0)
			kprintf(hex ? "-0x%x" : "-%u", res->r_base + res->r_length);
		kprintf(" ");
	}
}

/* vim:set ts=2 sw=2: */
